/**************************************************************************

Filename    :   RHI_HAL.cpp
Content     :   RHI Renderer HAL Prototype implementation.
Created     :   May 2009
Authors     :

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#if XBOX

#include "RHI_HAL.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Render/Render_BufferGeneric.h"
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

void HAL::SetDisplayPass ( DisplayPass pass )
{
	CurrentPass = pass;

	// If on the final display pass, ensure that the current target is set to the top level
	// render target on the stack (prepass/user code might have switched this).
	if ( CurrentPass == Display_Final && RenderTargetStack.GetSize() > 0 )
	{
		notifyHandlers ( HAL_FinalPassBegin );

		if ( VP.GetClippedRect ( &ViewRect ) )
		{
			HALState |= HS_ViewValid;
		}
		else
		{
			HALState &= ~HS_ViewValid;
		}
		updateViewport();
	}
}

void HAL::PushFilters ( FilterPrimitive* prim )
{
	if ( !checkState ( HS_InDisplay, __FUNCTION__ ) )
	{
		return;
	}

	FilterStackEntry e = {prim, 0};

	// Cue the profiler off of whether masks should be draw or not.
	if ( !Profiler.ShouldDrawMask() )
	{
		Profiler.SetDrawMode ( 2 );

		unsigned fillflags = FillFlags;
		ShaderPair pso = SManager.SetFill ( PrimFill_SolidColor, fillflags, PrimitiveBatch::DP_Batch, pVertexXY16IBatch, &ShaderData );
		RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );
		ShaderData.SetMatrix ( pso, Uniform::SU_mvp, prim->GetFilterAreaMatrix().GetMatrix2D() );
		const float white[] = {1, 1, 1, 1};
		ShaderData.SetUniform ( pso, Uniform::SU_cxmul, white, 4 );
		ShaderData.Finish ( 1 );
		RHIDrawPrimitive ( PT_TriangleList, 0, 2 );

		FilterStack.PushBack ( e );
		return;
	}

	// If the render target failed to allocate, don't draw items within the filter.
	if ( ( HALState & HS_CachedFilter ) ||
			( CurrentPass == Display_Prepass && FilterStack.GetSize() > 0 && !FilterStack.Back().pRenderTarget ) )
	{
		FilterStack.PushBack ( e );
		return;
	}

	// If there are already filters on the stack, we need to resolve them, as we may overwrite their EDRAM.
	if ( CurrentPass == Display_Prepass && FilterStack.GetSize() > 0 )
	{
		RenderTargetData* RTData = ( RenderTargetData* ) FilterStack.Back().pRenderTarget->GetHALData();
		FResolveParams params;
		params.ResolveTarget = RTData->Resource.TextureRHI;
		RHICopyToResolveTarget ( RTData->Resource.ColorBuffer, FALSE, params );
	}

	HALState |= HS_DrawingFilter;

	// Accept all rendering commands from now on (so we can draw the object that will be filtered).
	QueueProcessor.SetQueueEmitFilter ( RenderQueueProcessor::QPF_All );

	if ( prim->GetCacheState() ==  FilterPrimitive::Cache_Uncached && CurrentPass == Display_Prepass )
	{
		// Draw the filter from scratch.
		const Matrix2F& m = e.pPrimitive->GetFilterAreaMatrix().GetMatrix2D();
		e.pRenderTarget = *CreateTempRenderTarget ( ImageSize ( ( UInt32 ) m.Sx(), ( UInt32 ) m.Sy() ), prim->GetMaskPresent() );
		if ( e.pRenderTarget )
		{
			RectF frameRect ( m.Tx(), m.Ty(), m.Tx() + m.Sx(), m.Ty() + m.Sy() );
			PushRenderTarget ( frameRect, e.pRenderTarget );
			applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, false, true );

			// Disable masking from previous target, if this filter primitive doesn't have any masking.
			if ( MaskStackTop != 0 && !prim->GetMaskPresent() )
			{
				RHISetColorWriteEnable ( TRUE );
				RHISetStencilState ( TStaticStencilState<FALSE>::GetRHI() );
			}

			// If this primitive has masking, then clear the entire area to the current mask level, because
			// the depth stencil target may be different, and thus does not contain the previously written values.
			if ( prim->GetMaskPresent() )
			{
				RHIClear ( FALSE, FLinearColor::Black, FALSE, 0, TRUE, MaskStackTop );
			}
		}
	}
	else if ( prim->GetCacheState() != FilterPrimitive::Cache_Uncached )
	{
		// Drawing a cached filter, ignore all draw calls until the corresponding PopFilters.
		// Keep track of the level at which we need to draw the cached filter, by adding entries to the stack.
		SF_ASSERT ( prim->GetCacheState() == FilterPrimitive::Cache_Target );
		HALState |= HS_CachedFilter;
		CachedFilterIndex = FilterStack.GetSize();
		QueueProcessor.SetQueueEmitFilter ( RenderQueueProcessor::QPF_Filters );
	}
	else
	{
		static bool uncachedWarning = true;
		SF_DEBUG_WARNING ( uncachedWarning, "Attempt to draw uncached filter on final pass." );
		uncachedWarning = false;
	}
	FilterStack.PushBack ( e );
}

void HAL::PopFilters()
{
	FilterStackEntry e;
	e = FilterStack.Pop();

	if ( !Profiler.ShouldDrawMask() )
	{
		if ( FilterStack.GetSize() == 0 )
		{
			Profiler.SetDrawMode ( 0 );
		}
		return;
	}

	// If doing a cached filter, and haven't reached the level at which it will be displayed, ignore the pop.
	if ( ( HALState & HS_CachedFilter ) && ( CachedFilterIndex < ( int ) FilterStack.GetSize() ) )
	{
		return;
	}

	CachedFilterIndex = -1;
	if ( HALState & HS_CachedFilter )
	{
		// Only actually draw the top-level cached filter on the final pass.
		if ( CurrentPass == Display_Final || FilterStack.GetSize() > 0 )
		{
			drawCachedFilter ( e.pPrimitive );
			QueueProcessor.SetQueueEmitFilter ( RenderQueueProcessor::QPF_All );
		}
		else
		{
			QueueProcessor.SetQueueEmitFilter ( RenderQueueProcessor::QPF_Filters );
		}
		HALState &= ~HS_CachedFilter;
	}
	else
	{
		drawUncachedFilter ( e );

		// If there are no filters left in the stack, then go back to only accepting filter calls.
		if ( FilterStack.GetSize() == 0 && CurrentPass == Display_Prepass )
		{
			QueueProcessor.SetQueueEmitFilter ( RenderQueueProcessor::QPF_Filters );
		}
	}

	if ( FilterStack.GetSize() == 0 )
	{
		HALState &= ~HS_DrawingFilter;
	}

}

void HAL::drawUncachedFilter ( const FilterStackEntry& e )
{
	// X360 requires filters to be done in a prepass. If we are not in the prepass,
	// it means that the filtered content failed caching in the prepass, and the content
	// should be rendered unfiltered.
	if ( CurrentPass != Display_Prepass )
	{
		return;
	}

	const FilterSet* filters = e.pPrimitive->GetFilters();
	unsigned filterCount = filters->GetFilterCount();
	const Filter* filter = 0;
	unsigned pass = 0, passes = 0;

	// Invalid primitive or rendertarget.
	if ( !e.pPrimitive || !e.pRenderTarget || !e.pRenderTarget->GetTexture() )
	{
		return;
	}

	if ( GEmitDrawEvents )
	{
		appBeginDrawEvent ( FColor ( 180, 0, 180 ), TEXT ( "GFxDrawFilter" ) );
	}

	// Bind the render target.
	SF_ASSERT ( RenderTargetStack.Back().pRenderTarget == e.pRenderTarget );
	const int MaxTemporaryTextures = 3;
	Ptr<RenderTarget> temporaryTextures[MaxTemporaryTextures];
	appMemset ( temporaryTextures, 0, sizeof temporaryTextures );

	// Fill our the source target.
	ImageSize size = e.pRenderTarget->GetSize();
	temporaryTextures[Target_Source] = e.pRenderTarget;

	// Resolve the render target.
	RenderTargetData* RTData = ( RenderTargetData* ) temporaryTextures[0]->GetHALData();
	FResolveParams params;
	params.ResolveTarget = RTData->Resource.TextureRHI;
	RHICopyToResolveTarget ( RTData->Resource.ColorBuffer, FALSE, params );

	RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );

	// Overlay mode isn't actually supported, it contains the blend mode for filter sub-targets.
	applyBlendMode ( Blend_Overlay, true, false );
	RHISetColorWriteEnable ( TRUE );

	// Render filter(s).
	unsigned shaders[ShaderManager::MaximumFilterPasses];
	for ( unsigned i = 0; i < filterCount; ++i )
	{
		filter = filters->GetFilter ( i );
		passes = SManager.GetFilterPasses ( filter, FillFlags, shaders );

		// All shadows (except those hiding the object) need the original texture.
		bool requireSource = false;
		if ( filter->GetFilterType() >= Filter_Shadow &&
				filter->GetFilterType() <= Filter_Blur_End &&
				! ( ( ( BlurFilterImpl* ) filter )->GetParams().Mode & BlurFilterParams::Mode_HideObject ) )
		{
			temporaryTextures[Target_Original] = temporaryTextures[Target_Source];
			requireSource = true;
		}

		// Now actually render the filter.
		for ( pass = 0; pass < passes; ++pass )
		{
			// Create a destination texture if required.
			if ( !temporaryTextures[Target_Destination] )
			{
				temporaryTextures[Target_Destination] = *CreateTempRenderTarget ( size, false );

				// Failed allocation. Try and quit gracefully.
				if ( !temporaryTextures[Target_Destination] )
				{
					temporaryTextures[Target_Source]->SetInUse ( false );
					temporaryTextures[Target_Source] = 0;
					i = filterCount;
					break;
				}
			}

			RHISetRenderTarget ( ( ( RenderTargetData* ) temporaryTextures[Target_Destination]->GetHALData() )->Resource.ColorBuffer, 0 );
			RHIClear ( TRUE, FColor ( 0 ), FALSE, 0.0f, FALSE, 0 );
			++AccumulatedStats.RTChanges;

			// Scale to the size of the destination.
			RenderTarget* prt = temporaryTextures[1];
			const Rect<int>& viewRect = prt->GetRect(); // On the render texture, might not be the entire surface.
			const ImageSize& bs = prt->GetBufferSize();
			VP = Viewport ( bs.Width, bs.Height, viewRect.x1, viewRect.y1, viewRect.Width(), viewRect.Height() );
			ViewRect = Rect<int> ( viewRect.x1, viewRect.y1, viewRect.x2, viewRect.y2 );
			HALState |= HS_ViewValid;
			updateViewport();

			Matrix2F mvp = Matrix2F::Scaling ( 2, -2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
			mvp.Tx() -= 2 * GPixelCenterOffset / size.Width;
			mvp.Ty() += 2 * GPixelCenterOffset / size.Height;

			SManager.SetFilterFill ( mvp, Cxform::Identity, filter, temporaryTextures, shaders, pass, passes, pVertexXY16IBatch, &ShaderData );

			RHIDrawPrimitive ( PT_TriangleList, 0, 2 );

			// Clear textures, as they may be released immediately.
			RHISetSamplerState ( FPixelShaderRHIParamRef ( 0 ), 0, 0, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 0, 0, 0, 0, 0 );
			RHISetSamplerState ( FPixelShaderRHIParamRef ( 0 ), 1, 0, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 0, 0, 0, 0, 0 );

			// If we require the original source, save it, and create an additional destination target.
			if ( requireSource && pass == 0 )
			{
				temporaryTextures[Target_Original] = temporaryTextures[Target_Source];
				if ( passes > 1 )
				{
					temporaryTextures[Target_Source] = *CreateTempRenderTarget ( size, false );
				}
			}
			// On the last pass of each filter, release the original texture.
			if ( temporaryTextures[Target_Original] && pass == passes - 1 )
			{
				temporaryTextures[Target_Original]->SetInUse ( false );
			}

			// Resolve the render target.
			RenderTargetData* RTData1 = ( RenderTargetData* ) temporaryTextures[Target_Destination]->GetHALData();
			FResolveParams params;
			params.ResolveTarget = RTData1->Resource.TextureRHI;
			RHICopyToResolveTarget ( RTData1->Resource.ColorBuffer, FALSE, params );

			// Setup for the next pass.
			Alg::Swap ( temporaryTextures[Target_Source], temporaryTextures[Target_Destination] );
		}

		AccumulatedStats.Primitives += passes;
		AccumulatedStats.Meshes     += passes;
	}

	// Cache the last step, as it will be used to draw the filtered object in the final pass (and
	// potentially used on subsequent frames).
	if ( temporaryTextures[Target_Source] )
	{
		RenderTarget* cacheResults[1] = { temporaryTextures[Target_Source] };
		e.pPrimitive->SetCacheResults ( FilterPrimitive::Cache_Target, cacheResults, 1 );
		( ( RenderTargetData* ) cacheResults[0]->GetHALData() )->CacheID = reinterpret_cast<UPInt> ( e.pPrimitive.GetPtr() );
	}

	// Pop the temporary target.
	PopRenderTarget();

	// If this is not the top filter on the stack, we need to draw it into the stacked filtered target,
	// plus the content we previously resolved for the higher level stack.
	if ( FilterStack.GetSize() > 0 && temporaryTextures[Target_Source] )
	{
		// Previous content.
		Matrix4F mvp = Matrix2F::Scaling ( 2, -2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
		unsigned fillFlags = FillFlags & FF_Multiply;
		const ShaderManager::Shader& pso = SManager.SetFill (
										   PrimFill_Texture, fillFlags, PrimitiveBatch::DP_Single, pVertexXY16IBatch, &ShaderData );

		RenderTarget* prt = FilterStack.Back().pRenderTarget;
		Texture* pt = ( Texture* ) prt->GetTexture();
		RenderTargetData* phd = ( RenderTargetData* ) ( prt->GetHALData() );
		Rect<int> srect = FilterStack.Back().pRenderTarget->GetRect();

		Matrix2F texgen;
		texgen.AppendTranslation ( ( float ) srect.x1, ( float ) srect.y1 );
		texgen.AppendScaling ( ( float ) srect.Width() / pt->GetSize().Width, ( float ) srect.Height() / pt->GetSize().Height );

		ShaderData.SetUniform ( pso, Uniform::SU_mvp, &mvp.M[0][0], 8 );
		ShaderData.SetUniform ( pso, Uniform::SU_texgen, &texgen.M[0][0], 8 );
		ShaderData.SetTexture ( pso, 0, pt, ImageFillMode ( Wrap_Clamp, Sample_Linear ) );
		ShaderData.Finish ( 0 );

		RHIClear ( TRUE, FLinearColor ( 0, 0, 0, 0 ), FALSE, 0, FALSE, 0 );
		applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, true, true );
		RHIDrawPrimitive ( PT_TriangleList, 0, 2 );
		applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, false );

		// And then the cached filter.
		drawCachedFilter ( e.pPrimitive );
	}

	if ( HALState & HS_DrawingMask )
	{
		RHISetColorWriteEnable ( TRUE );
	}

	// Cleanup. NOTE: We do not set the final target to not be in use, because we will require
	// it in the final pass rendering.
	for ( unsigned i = Target_Destination; i < MaxTemporaryTextures; ++i )
	{
		RHISetSamplerState ( FPixelShaderRHIParamRef ( 0 ), i, 0, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 0, 0, 0, 0, 0 );
		if ( temporaryTextures[i] )
		{
			temporaryTextures[i]->SetInUse ( false );
		}
	}
	AccumulatedStats.Filters += filters->GetFilterCount();

	if ( GEmitDrawEvents )
	{
		appEndDrawEvent();
	}
}

}
}
}
#endif

#endif//WITH_GFx