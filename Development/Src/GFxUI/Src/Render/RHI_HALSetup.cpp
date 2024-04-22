/**************************************************************************

Filename    :   RHI_HALSetup.cpp
Content     :   Renderer HAL Prototype header.
Created     :   May 2009
Authors     :   Michael Antonov

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#include "RHI_HAL.h"
#include "../../Engine/Src/SceneRenderTargets.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#if XBOX
#include "Render/RHI_XeBufferMemory.h"
#endif
#include "Render/Render_BufferGeneric.h"

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

// *** RenderHAL_RHI Implementation

// Cannot use MapVertexFormat for these, as it changes the size.
static VertexElement VertexXY16IBatch_E[] =
{
	{ 0, VET_XY16i },
#if XBOX
	{ 7, VET_Instance8 },
#else
	{ 4, VET_Instance8 },
#endif
	{ 0, VET_None }
};
static VertexFormat VertexXY16IBatch = { sizeof ( VertexXY16iInstance ), VertexXY16IBatch_E };

#if _WINDOWS
// DX11 formats must use F32 for positions, because of their vertex shader attributes.
static VertexElement VertexXY32FBatch_E[] =
{
    { 0, VET_XY32f },
    { 4, VET_Instance8 },
    { 0, VET_None }
};
static VertexFormat VertexXY32FBatch = { sizeof ( VertexXY32fInstance ), VertexXY32FBatch_E };
#endif


bool HAL::InitHAL ( const RHI::HALInitParams& params )
{
	if ( !Render::HAL::initHAL ( params ) )
	{
		return false;
	}

	SManager.Initialize();
	if ( !Cache.Initialize() )
	{
		return 0;
	}

	// Create Texture manager if needed.
	if ( params.pTextureManager )
	{
		pTextureManager = params.GetTextureManager();
	}
	else
	{
		pTextureManager =  *SF_HEAP_AUTO_NEW ( this ) TextureManager ( pRTCommandQueue );
		if ( !pTextureManager )
		{
			Cache.Reset();
			return 0;
		}
	}

	pRenderBufferManager = params.pRenderBufferManager;
	if ( !pRenderBufferManager )
	{
#if SF_RENDERBUFFERMEMORY_ENABLE
		pRenderBufferManager = *SF_HEAP_AUTO_NEW ( this ) RenderBufferManagerMemory();
#else
		pRenderBufferManager = *SF_HEAP_AUTO_NEW ( this ) RenderBufferManagerGeneric();
#endif

#if CONSOLE
		ImageSize ScreenSize = ImageSize ( GScreenWidth, GScreenHeight );
#else
		ImageSize ScreenSize = ImageSize ( GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY() );
#endif
		pRenderBufferManager->Initialize ( pTextureManager, Image_R8G8B8A8, ScreenSize );
	}

	FStencilStateInitializerRHI Initializer (
	    TRUE,      // bEnableFrontFaceStencil
	    CF_Always, //FrontFaceStencilTest
	    SO_Replace,// FrontFaceStencilFailStencilOp
	    SO_Replace,// FrontFaceDepthFailStencilOp
	    SO_Replace,// FrontFacePassStencilOp
	    FALSE,     // bEnableBackFaceStencil
	    CF_Always, // BackFaceStencilTest
	    SO_Keep,   // BackFaceStencilFailStencilOp
	    SO_Keep,   // BackFaceDepthFailStencilOp
	    SO_Keep,   // BackFacePassStencilOp
	    0xFFFFFFFF,// StencilReadMask
	    0xFFFFFFFF,// StencilWriteMask
	    0          // StencilRef
	);
	MaskClearStencilStateRHI = RHICreateStencilState ( Initializer );

	{
		if ( !VertexXY16IBatch.pSysFormat )
		{
#if _WINDOWS
            if ( GRHIShaderPlatform == SP_PCD3D_SM4 || GRHIShaderPlatform == SP_PCD3D_SM5 )
            {
                VertexXY32FBatch.pSysFormat = * ( Render::SystemVertexFormat* ) SF_NEW SysVertexFormat ( &VertexXY32FBatch );
                pVertexXY16IBatch = &VertexXY32FBatch;
            }
            else
#endif
            {
    			VertexXY16IBatch.pSysFormat = * ( Render::SystemVertexFormat* ) SF_NEW SysVertexFormat ( &VertexXY16IBatch );
                pVertexXY16IBatch = &VertexXY16IBatch;
            }
		}
	}

	HALState |= HS_ModeSet;

	notifyHandlers ( HAL_Initialize );
	return true;
}

// Returns back to original mode (cleanup)
bool HAL::ShutdownHAL()
{
	if ( ! ( HALState & HS_ModeSet ) )
	{
		return true;
	}
	notifyHandlers ( HAL_Shutdown );

	destroyRenderBuffers();
	pRenderBufferManager.Clear();

	pTextureManager->Reset();
	pTextureManager.Clear();
	//SManager.Reset();
	Cache.Reset();

	// prevent problem on shutdown
	VertexXY16IBatch.pSysFormat = NULL;
	
	// Remove ModeSet and other state flags.
	HALState = 0;
	return true;
}


// ***** Rendering

bool HAL::BeginScene()
{
	if ( !checkState ( HS_InFrame, "BeginScene" ) )
	{
		return false;
	}
	HALState |= HS_InScene;
	Profiler.SetProfileViews ( NextProfileMode );

	// Blending render states.
	applyBlendMode ( Blend_None );

	// Disable depth testing and culling
	RHISetDepthState ( TStaticDepthState<FALSE, CF_Always>::GetRHI() );
	RHISetRasterizerState ( TStaticRasterizerState<FM_Solid, CM_None>::GetRHI() );
	CurStencilStateRHI = TStaticStencilState<FALSE>::GetRHI();
	RHISetStencilState ( CurStencilStateRHI );

	//SManager.BeginScene();
	//pTextureManager->BeginScene();

	return true;
}

void HAL::EndScene()
{
	if ( !checkState ( HS_InFrame | HS_InScene, "BeginScene" ) )
	{
		return;
	}

	Flush();


	//pTextureManager->EndScene();
	//SManager.EndScene();

	HALState &= ~HS_InScene;
}



void HAL::beginDisplay ( BeginDisplayData* InData )
{
	if ( GEmitDrawEvents )
	{
		appBeginDrawEvent ( FColor ( 180, 0, 180 ), TEXT ( "GFxBeginDisplay" ) );
	}

	Render::HAL::beginDisplay ( InData );

	// The first prepass may not have a render target set
	if ( RenderTargetStack.GetSize() > 0 )
	{
		RenderTargetData* RT = ( RenderTargetData* ) RenderTargetStack.Back().pRenderTarget->GetHALData();
		RT->Resource.SetGammaFromViewport ( InData->VP );
	}
}

void HAL::endDisplay()
{
	Render::HAL::endDisplay();

	if ( GEmitDrawEvents )
	{
		appEndDrawEvent();
	}
}

RenderTargetResource::RenderTargetResource ( RHI::Texture* InTexture, ImageSize InSize )
	:   Size ( InSize )
	,   InverseGamma ( 1.0f )
	,   Owner ( NULL )
	,   Texture ( InTexture )
{
	InitResource();
}

RenderTargetResource::RenderTargetResource ( FRenderTarget* InOwner, ImageSize InSize )
	:   Size ( InSize )
	,   InverseGamma ( 1.0f )
	,   Owner ( InOwner )
	,   Texture ( NULL )
{
	InitResource();
}

RenderTargetResource::~RenderTargetResource()
{
	check ( IsInRenderingThread() );
	ReleaseResource();
}

void RenderTargetResource::InitDynamicRHI()
{
	if ( Texture )
	{
		if ( Texture->State == Texture::State_Lost )
		{
			Texture->Initialize();
		}

		// Texture::InitDynamicRHI is safe to call multiple times
		Texture->pTextures[0].Resource->InitDynamicRHI();
		TextureRHI = Texture->pTextures[0].Get2DRHI();
		if ( Texture->pTextures[0].Tex && IsValidRef ( Texture->pTextures[0].Tex->SurfaceRHI ) )
		{
			ColorBuffer = Texture->pTextures[0].Tex->SurfaceRHI;
		}
		else
		{
            ColorBuffer = RHICreateTargetableSurface ( Size.Width, Size.Height, PF_A8R8G8B8, TextureRHI, 0, TEXT ( "GFxTempColor" ) );
		}
		InverseGamma = 1;
	}
	else if (Owner)
	{
		ColorBuffer = Owner->GetRenderTargetSurface();

		// Gamma correction
		if ( Owner->GetDisplayGamma() > 0 )
		{
			InverseGamma = 1.0f / Owner->GetDisplayGamma();
		}
		else
		{
			InverseGamma = 1.0f / 2.2f;
		}
	}
}

void RenderTargetResource::SetGammaFromViewport ( const Viewport& VP )
{
	if ( Texture )
	{
		return;
	}

	INT GammaMode;
	if ( VP.Flags & Viewport_NoGamma )
	{
		GammaMode = 0;
	}
	else if ( VP.Flags & Viewport_InverseGamma )
	{
		GammaMode = -1;
	}
	else
	{
		GammaMode = 1;
	}

	if ( GammaMode < 0 )
	{
		if ( Owner->GetDisplayGamma() > 0 )
		{
			InverseGamma = Owner->GetDisplayGamma();
		}
		else
		{
			InverseGamma = 2.2f;
		}
	}
	else if ( GammaMode > 0 )
	{
		if ( Owner->GetDisplayGamma() > 0 )
		{
			InverseGamma = 1.0f / Owner->GetDisplayGamma();
		}
		else
		{
			InverseGamma = 1.0f / 2.2f;
		}
	}
	else
	{
		InverseGamma = 1.0f;
	}
}

void RenderTargetResource::ReleaseDynamicRHI()
{
	TextureRHI.SafeRelease();
	ColorBuffer.SafeRelease();

	if ( Texture && Texture->Type != Texture::Type_Managed &&
	        Texture->pTextures[0].Resource)
	{
		Texture->pTextures[0].Resource->ReleaseDynamicRHI();
	}
}

static FSceneDepthTargetProxy SceneDepth;

RenderTarget* HAL::GetDefaultRenderTarget()
{
	if ( RenderTargetStack.GetSize() == 0 )
	{
		return 0;
	}
	return RenderTargetStack[0].pRenderTarget;
}

RenderTarget* HAL::CreateRenderTarget ( FRenderTarget* InRT, bool useSceneDepth )
{
	RenderTarget* NewRT = pRenderBufferManager->CreateRenderTarget ( ImageSize ( InRT->GetSizeX(), InRT->GetSizeY() ),
	                      RBuffer_User, Image_R8G8B8A8, NULL );
	Ptr<DepthStencilSurface> pds = 0;
#if WITH_MOBILE_RHI
    if ( GUsingES2RHI )
    {
        useSceneDepth = false;
    }
#endif
    if ( useSceneDepth )
    {
		ImageSize size ( InRT->GetSizeX(), InRT->GetSizeY() );
	    pds = *pTextureManager->CreateDepthStencilSurface ( size );
    	pds->Resource.Initialize ( &SceneDepth );
	}

	RenderTargetData::UpdateData ( NewRT, InRT, 0, 0, pds );
	return NewRT;
}

RenderTarget* HAL::CreateRenderTargetFromViewport ( FViewport* InRT, bool useSceneDepth )
{
    RenderTarget* NewRT = pRenderBufferManager->CreateRenderTarget ( ImageSize ( InRT->GetSizeX(), InRT->GetSizeY() ),
                                                                     RBuffer_User, Image_R8G8B8A8, NULL );
    Ptr<DepthStencilSurface> pds = 0;
    if ( useSceneDepth )
    {
#if WITH_ES2_RHI && !USE_NULL_RHI
        if ( GUsingES2RHI )
        {
            FSurfaceRHIRef SurfaceRHI = InRT->GetRenderTargetSurface();
            UINT SizeX, SizeY;
            RHIGetTargetSurfaceSize ( SurfaceRHI, SizeX, SizeY );
            ImageSize size ( SizeX, SizeY );
            pds = *pTextureManager->CreateDepthStencilSurface ( size );
            pds->Resource.Initialize ( InRT );
        }
        else
#endif
        {
            ImageSize size ( InRT->GetSizeX(), InRT->GetSizeY() );
            pds = *pTextureManager->CreateDepthStencilSurface ( size );
            pds->Resource.Initialize ( &SceneDepth );
        }
    }

    RenderTargetData::UpdateData ( NewRT, InRT, 0, 0, pds );
    return NewRT;
}

void HAL::UpdateRenderTarget ( RenderTarget* InRT )
{
	RenderTargetData* RTData = ( RenderTargetData* ) InRT->GetHALData();
	check ( RTData->Resource.Owner );
	RTData->Resource.ReleaseDynamicRHI();
	RTData->Resource.InitDynamicRHI();
	if ( RTData->DepthStencil && !RTData->DepthStencil->Resource.bIsAllocated )
	{
		RTData->DepthStencil->Resource.ReleaseDynamicRHI();
		RTData->DepthStencil->Resource.InitDynamicRHI();
	}
}

RenderTarget* HAL::CreateRenderTarget ( Render::Texture* texture, bool needsStencil )
{
	RHI::Texture* pt = ( RHI::Texture* ) texture;

	// Cannot render to textures which have multiple HW representations.
	SF_ASSERT ( pt->TextureCount == 1 );

	RenderTarget* NewRT = pRenderBufferManager->CreateRenderTarget ( texture->GetSize(), RBuffer_Texture, texture->GetFormat(), texture );
	Ptr<DepthStencilBuffer> DepthBuffer;

	if ( needsStencil )
	{
		DepthBuffer = *pRenderBufferManager->CreateDepthStencilBuffer ( NewRT->GetBufferSize() );
	}

	RenderTargetData::UpdateData ( NewRT, NULL, pt, DepthBuffer, 0 );
	return NewRT;
}

RenderTarget* HAL::CreateTempRenderTarget ( const ImageSize& size, bool needsStencil )
{
	RenderTarget* prt = pRenderBufferManager->CreateTempRenderTarget ( size );
	if ( !prt )
	{
		return 0;
	}
	if ( prt->GetHALData() )
	{
		return prt;
	}

	RHI::Texture* pt = ( RHI::Texture* ) prt->GetTexture();

	// Cannot render to textures which have multiple HW representations.
	SF_ASSERT ( pt->TextureCount == 1 );

	Ptr<DepthStencilBuffer> DepthBuffer;
	if ( needsStencil )
	{
		DepthBuffer = *pRenderBufferManager->CreateDepthStencilBuffer ( prt->GetBufferSize() );
	}

	RenderTargetData::UpdateData ( prt, NULL, pt, DepthBuffer, 0 );
	return prt;
}

bool HAL::SetRenderTarget ( RenderTarget* ptarget, bool setState )
{
	// Cannot set the bottom level render target if already in display.
	if ( HALState & HS_InDisplay )
	{
		return false;
	}

	// When changing the render target while in a scene, we must flush all drawing.
	if ( HALState & HS_InScene )
	{
		Flush();
	}

	RenderTargetEntry entry;

	if ( setState )
	{
		RenderTargetData* prtdata = ( RHI::RenderTargetData* ) ptarget->GetHALData();
		RHISetRenderTarget ( prtdata->Resource.ColorBuffer, prtdata->DepthStencil ? prtdata->DepthStencil->Resource.DepthBuffer : 0 );
		++AccumulatedStats.RTChanges;
	}

	if ( !ptarget )
	{
		if ( RenderTargetStack.GetSize() > 0 )
		{
			RenderTargetStack.PopBack();
		}
		return true;
	}

	entry.pRenderTarget = ptarget;

	// Replace the stack entry at the bottom, or if the stack is empty, add one.
	if ( RenderTargetStack.GetSize() > 0 )
	{
		RenderTargetStack[0] = entry;
	}
	else
	{
		RenderTargetStack.PushBack ( entry );
	}
	return true;
}

void HAL::PushRenderTarget ( const RectF& frameRect, RenderTarget* prt )
{
	HALState |= HS_InRenderTarget;
	RenderTargetEntry entry = {prt, Matrices, ViewRect, VP};
	Matrices.SetUserMatrix ( Matrix2F::Identity );

	// Setup the render target/depth stencil on the device.
	if ( !prt )
	{
		SF_DEBUG_WARNING ( 1, "PushRenderTarget - invalid render target." );
		RenderTargetStack.PushBack ( entry );
		return;
	}

	RenderTargetData* phd = ( RHI::RenderTargetData* ) prt->GetHALData();
	++AccumulatedStats.RTChanges;
	UBOOL bClearStencil = FALSE;

	if ( phd->DepthStencil )
	{
		RHISetRenderTarget ( phd->Resource.ColorBuffer, phd->DepthStencil->Resource.DepthBuffer );
		bClearStencil = TRUE;
	}
	else
	{
		RHISetRenderTarget ( phd->Resource.ColorBuffer, 0 );
	}

	// Setup viewport.
	Rect<int> viewRect = prt->GetRect(); // On the render texture, might not be the entire surface.
	const ImageSize& bs = prt->GetBufferSize();
	VP = Viewport ( bs.Width, bs.Height, viewRect.x1, viewRect.y1, viewRect.Width(), viewRect.Height() );
    VP.Flags |= Viewport::View_IsRenderTexture;
	ViewRect.x1 = ( int ) frameRect.x1;
	ViewRect.y1 = ( int ) frameRect.y1;
	ViewRect.x2 = ( int ) frameRect.x2;
	ViewRect.y2 = ( int ) frameRect.y2;

#if NGP
	RHISetMobileGFxParams( EGFXBM_None );
	RHISetStencilState ( MaskClearStencilStateRHI );

    clearSolidRectangle ( Rect<int>(bs.Width*40, bs.Height*40), 0 );
	applyBlendMode( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, false, ( HALState & HS_InRenderTarget ) != 0 );

	RHISetStencilState ( CurStencilStateRHI );
#else
    RHIClear ( TRUE, FColor ( 0 ), FALSE, 0.0f, bClearStencil, 0 );
#endif

	// Must offset the 'original' viewrect, otherwise the 3D compensation matrix will be offset.
	Matrices.ViewRectOriginal.Offset ( -entry.OldViewport.Left, -entry.OldViewport.Top );
	Matrices.UVPOChanged = true;
	HALState |= HS_ViewValid;
	updateViewport();

	RenderTargetStack.PushBack ( entry );
}

void HAL::PopRenderTarget()
{
	RenderTargetEntry& entry = RenderTargetStack.Back();
	RenderTarget* prt = entry.pRenderTarget;

#if !XBOX
	RenderTargetData* ResolveRT = ( RenderTargetData* ) prt->GetHALData();
    RHICopyToResolveTarget( ResolveRT->Resource.ColorBuffer, FALSE, FResolveParams(ResolveRT->Resource.TextureRHI) );
#endif

    if ( prt->GetType() == RBuffer_Temporary )
	{
		// Strip off the depth stencil surface/buffer from temporary targets.
		RHI::RenderTargetData* plasthd = ( RenderTargetData* ) prt->GetHALData();
		if ( plasthd->DepthStencil )
		{
			plasthd->DepthStencil = 0;
		}
		plasthd->pDepthStencilBuffer = 0;
	}

	Matrices = entry.OldMatrixState;
	ViewRect = entry.OldViewRect;
	VP = entry.OldViewport;

	// Must reverse the offset of the 'original' viewrect.
	Matrices.ViewRectOriginal.Offset ( entry.OldViewport.Left, entry.OldViewport.Top );
	Matrices.UVPOChanged = true;

	RenderTargetStack.PopBack();
	RenderTargetData* phd = 0;
	RenderTargetData* RTDepth = 0;
	if ( RenderTargetStack.GetSize() > 0 )
	{
		RenderTargetEntry& back = RenderTargetStack.Back();
		phd = ( RHI::RenderTargetData* ) back.pRenderTarget->GetHALData();
	}

	if ( RenderTargetStack.GetSize() <= 1 )
	{
		HALState &= ~HS_InRenderTarget;
	}

#if XBOX
	if ( CurrentPass != Display_Prepass || HALState & HS_InRenderTarget )
#endif
	{
		if ( phd->DepthStencil )
		{
			RHISetRenderTarget ( phd->Resource.ColorBuffer, phd->DepthStencil->Resource.DepthBuffer );
		}
		else
		{
			RHISetRenderTarget ( phd->Resource.ColorBuffer, 0 );
		}
	}

	++AccumulatedStats.RTChanges;

	// Reset the viewport to the last render target on the stack.
	HALState |= HS_ViewValid;
	updateViewport();
}

void HAL::updateViewport()
{
	if ( HALState & HS_ViewValid )
	{
		int dx = ViewRect.x1 - VP.Left,
		    dy = ViewRect.y1 - VP.Top;

		// Modify HW matrix and viewport to clip.
		CalcHWViewMatrix ( VP.Flags, &Matrices.View2D, ViewRect, dx, dy );
		Matrices.SetUserMatrix ( Matrices.User );
		Matrices.ViewRect    = ViewRect;
		Matrices.UVPOChanged = 1;

		if ( HALState & HS_InRenderTarget )
		{
		    RHISetViewport ( VP.Left, VP.Top, 0, VP.Left + VP.Width, VP.Top + VP.Height, 0 );
        }
		else
		{
			Viewport vp = VP;
			vp.Left     = ViewRect.x1;
			vp.Top      = ViewRect.y1;
			vp.Width    = ViewRect.Width();
			vp.Height   = ViewRect.Height();
			vp.SetStereoViewport(Matrices.S3DDisplay);
			RectF vpRect;
			vpRect.SetRect(vp.Left, vp.Top, vp.Left + vp.Width, vp.Top + vp.Height);

			RHISetViewport ( 
				(UINT)(vpRect.x1), 
				(UINT)(vpRect.y1), 
				0, 
				(vpRect.x2 == vpRect.x1) ? (UINT)(vpRect.x1) + 1 : (UINT)(vpRect.x2),
				(vpRect.y2 == vpRect.y1) ? (UINT)(vpRect.y1) + 1 : (UINT)(vpRect.y2),
				0 );		
		}
	}
	else
	{
		RHISetViewport ( 0, 0, 0, 0, 0, 0 );
	}
}

void HAL::CalcHWViewMatrix ( unsigned VPFlags, Matrix* pmatrix,
                             const Rect<int>& viewRect, int dx, int dy )
{
	float vpWidth = ( float ) viewRect.Width();
	float vpHeight = ( float ) viewRect.Height();

	float xhalfPixelAdjust = ( viewRect.Width() > 0 ) ? ( ( 2.0f * GPixelCenterOffset ) / vpWidth ) : 0.0f;
	float yhalfPixelAdjust = ( viewRect.Height() > 0 ) ? ( ( 2.0f * GPixelCenterOffset ) / vpHeight ) : 0.0f;

	pmatrix->SetIdentity();

#if PLATFORM_DESKTOP || MOBILE && !NGP
	if (
#if _WINDOWS
        (GRHIShaderPlatform == SP_PCOGL || GUsingMobileRHI) &&
#endif
        (VPFlags & Viewport::View_IsRenderTexture) )
	{
		pmatrix->Sx() = 2.0f  / vpWidth;
		pmatrix->Sy() = 2.0f /  vpHeight;
		pmatrix->Tx() = -1.0f - pmatrix->Sx() * ((float)dx);
		pmatrix->Ty() = -1.0f - pmatrix->Sy() * ((float)dy);
	}
	else
#endif
	{
		pmatrix->Sx() = 2.0f  / vpWidth;
		pmatrix->Sy() = -2.0f / vpHeight;
		pmatrix->Tx() = -1.0f - pmatrix->Sx() * ( ( float ) dx ) - xhalfPixelAdjust;
		pmatrix->Ty() = 1.0f  - pmatrix->Sy() * ( ( float ) dy ) + yhalfPixelAdjust;
	}
}


}
}
} // Scaleform::Render::RHI

#endif//WITH_GFx
