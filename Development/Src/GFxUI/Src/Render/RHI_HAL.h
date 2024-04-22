/**************************************************************************

Filename    :   RHI_HAL.h
Content     :   Renderer HAL Prototype header.
Created     :   May 2009
Authors     :

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef INC_SF_Render_RHI_HAL_H
#define INC_SF_Render_RHI_HAL_H

#if WITH_GFx

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Render/Render_HAL.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#include "Render/RHI_MeshCache.h"
#include "Render/RHI_Shader.h"
#include "Render/RHI_Texture.h"


namespace Scaleform
{
namespace Render
{
namespace RHI
{

enum EGViewportFlags
{
	Viewport_NoGamma       = Render::Viewport::View_FirstHalFlag,
	Viewport_InverseGamma  = Render::Viewport::View_FirstHalFlag << 1,
};


// RHI::HALInitParems provides RHI-specific rendering initialization
// parameters for HAL::InitHAL.

struct HALInitParams : public Render::HALInitParams
{
	HALInitParams ( UInt32 halConfigFlags = 0,
	                ThreadId renderThreadId = ThreadId() )
		: Render::HALInitParams ( 0, halConfigFlags, renderThreadId )
	{
	}

	// RHI::TextureManager accessors for correct type.
	void SetTextureManager ( TextureManager* manager )
	{
		pTextureManager = manager;
	}
	TextureManager* GetTextureManager() const
	{
		return ( TextureManager* ) pTextureManager.GetPtr();
	}
};

class RenderTargetResource : public FRenderResource
{
	public:
		ImageSize                       Size;
		//Actual allocated size of the buffer/texture backing this render target
		ImageSize                       AllocatedSize;

		float                           InverseGamma;
		FRenderTarget*                  Owner;
		Ptr<RHI::Texture>               Texture;
		FTexture2DRHIRef                TextureRHI;
		FSurfaceRHIRef                  ColorBuffer;

		RenderTargetResource ( FRenderTarget* InOwner, ImageSize InSize );
		RenderTargetResource ( RHI::Texture* InTexture, ImageSize InSize );
		~RenderTargetResource();

		// FRenderResource
		virtual void InitDynamicRHI();
		virtual void ReleaseDynamicRHI();

		void SetGammaFromViewport ( const Viewport& VP );
};

class RenderTargetData : public RenderBuffer::HALData
{
	public:
		RenderTargetResource     Resource;
		Ptr<DepthStencilSurface> DepthStencil;        // Used for user-created (i.e., game) render targets

		static void UpdateData ( RenderBuffer* InBuffer, FRenderTarget* InOwner, Texture* InTexture, DepthStencilBuffer* InDepthBuffer, DepthStencilSurface* InDepthSurface );
	private:
		RenderTargetData ( RenderBuffer* InBuffer, FRenderTarget* InOwner, DepthStencilBuffer* InDepthBuffer, DepthStencilSurface* InDepthSurface );
		RenderTargetData ( RenderBuffer* InBuffer, Texture* InTexture, DepthStencilBuffer* InDepthBuffer, DepthStencilSurface* InDepthSurface );
		virtual ~RenderTargetData();
};

class HAL : public Render::HAL
{
	public:
		UInt32				FillFlags;

		RHI::MeshCache      Cache;
		RenderQueueProcessor QueueProcessor;

		ShaderManager       SManager;
		ShaderInterface     ShaderData;
		Ptr<TextureManager> pTextureManager;

		const VertexFormat  *pVertexXY16IBatch;

		// Previous batching mode
		PrimitiveBatch::BatchType PrevBatchType;

		FStencilStateRHIRef MaskClearStencilStateRHI;
		FStencilStateRHIRef CurStencilStateRHI;

		// Self-accessor used to avoid constructor warning.
		HAL*      getThis()
		{
			return this;
		}

	public:


		HAL ( ThreadCommandQueue* commandQueue = 0 );
		virtual ~HAL();

		// *** HAL Initialization / Shutdown Logic

		// Initialize rendering
		virtual bool        InitHAL ( const RHI::HALInitParams& params );
		// Shutdown rendering (cleanup).
		virtual bool        ShutdownHAL();

		// RHI device Reset and lost device support.
		// - PrepareForReset should be called before IDirect3DDevice9::Reset to release
		// caches and other system-specific references.
		void                PrepareForReset();
		// - RestoreAfterReset called after reset to restore needed variables.
		bool                RestoreAfterReset();


		// *** Rendering

		virtual bool        BeginScene();
		virtual void        EndScene();

		// Bracket the displaying of a frame from a movie.
		// Fill the background color, and set up default transforms, etc.
		virtual void        beginDisplay ( BeginDisplayData* InData );
		virtual void        endDisplay();

		void                CalcHWViewMatrix ( unsigned VPFlags, Matrix* pmatrix, const Rect<int>& viewRect,
		                                       int dx, int dy );

		// Updates HW Viewport and ViewportMatrix based on the current
		// values of VP, ViewRect and ViewportValid.
		virtual void        updateViewport();


		// Creates / Destroys mesh and DP data

		virtual PrimitiveFill*  CreatePrimitiveFill ( const PrimitiveFillData& data );

		virtual void        DrawProcessedPrimitive ( Primitive* pprimitive,
		        PrimitiveBatch* pstart, PrimitiveBatch *pend );

		virtual void        DrawProcessedComplexMeshes ( ComplexMesh* p,
		        const StrideArray<HMatrix>& matrices );

		template< class MatrixUpdateAdapter >
		void                applyMatrixConstants ( const MatrixUpdateAdapter & input );

		void                applyRawMatrixConstants ( const Matrix& m, const Cxform& cx );

		template< class MatrixType >
		static void         calculateTransform ( const Matrix & m, const HMatrix& hm, const MatrixState & mstate, float ( * dest ) [4] );


		// *** Mask Support

		virtual void    PushMask_BeginSubmit ( MaskPrimitive* primitive );
		virtual void    EndMaskSubmit();
		virtual void    PopMask();

		bool    checkMaskBufferCaps();
		void    drawMaskClearRectangles ( const HMatrix* matrices, UPInt count );

		// Background clear helper, expects viewport coordinates.
		virtual void    clearSolidRectangle ( const Rect<int>& r, Color color );

		virtual bool    createDefaultRenderBuffer()
		{
			return true;
		}


		// *** BlendMode

		virtual void    applyBlendMode ( BlendMode NewMode, bool bSourceAc = false, bool bForceAc = false );

		virtual TextureManager* GetTextureManager() const
		{
			return pTextureManager.GetPtr();
		}

		virtual RenderTarget*   GetDefaultRenderTarget();
		virtual RenderTarget*   CreateRenderTarget ( FRenderTarget* InRT, bool useSceneDepth );
        virtual RenderTarget*   CreateRenderTargetFromViewport ( FViewport* InRT, bool useSceneDepth );

		virtual void            UpdateRenderTarget ( RenderTarget* InRT );
		virtual RenderTarget*   CreateRenderTarget ( Render::Texture* texture, bool needsStencil );
		virtual RenderTarget*   CreateTempRenderTarget ( const ImageSize& size, bool needsStencil );
		virtual bool            SetRenderTarget ( RenderTarget* target, bool setState = 1 );
		virtual void            PushRenderTarget ( const RectF& frameRect, RenderTarget* prt );
		virtual void            PopRenderTarget();

		// *** Filters
		virtual void            PushFilters ( FilterPrimitive* primitive );
		virtual void            PopFilters();

		virtual void            drawUncachedFilter ( const FilterStackEntry& e );
		virtual void            drawCachedFilter ( FilterPrimitive* primitive );

		virtual class MeshCache& GetMeshCache()
		{
				return Cache;
		}

		virtual RQCacheInterface& GetRQCacheInterface()
		{
			return QueueProcessor.GetQueueCachesRef();
		}
		virtual RenderQueueProcessor&  GetRQProcessor()
		{
			return QueueProcessor;
		}

		virtual void    MapVertexFormat ( PrimitiveFillType fill, const VertexFormat* sourceFormat,
		                                  const VertexFormat** single,
		                                  const VertexFormat** batch, const VertexFormat** instanced, unsigned );

#if XBOX
		virtual void SetDisplayPass ( DisplayPass pass );
#endif
};

}
}
} // Scaleform::Render::RHI

#endif//WITH_GFx

#endif
