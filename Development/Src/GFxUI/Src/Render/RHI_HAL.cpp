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

#include "RHI_HAL.h"


namespace Scaleform
{
namespace Render
{
namespace RHI
{

// ***** HAL_RHI

HAL::HAL ( ThreadCommandQueue* commandQueue )
	:   Render::HAL ( commandQueue ),
	    FillFlags ( 0 ),
	    Cache ( Memory::GetGlobalHeap(), (GUsingMobileRHI || MOBILE) ? MeshCacheParams(MeshCacheParams::Console_Defaults) : MeshCacheParams(MeshCacheParams::PC_Defaults) ),
	    QueueProcessor ( Queue, getThis() ),
	    SManager ( &Profiler/*, Cache.GetParams()*/ ),
	    ShaderData ( getThis() ),
	    PrevBatchType ( PrimitiveBatch::DP_None )
{
}

HAL::~HAL()
{
	ShutdownHAL();
}


PrimitiveFill*  HAL::CreatePrimitiveFill ( const PrimitiveFillData &data )
{
	return SF_HEAP_NEW ( pHeap ) PrimitiveFill ( data );
}

void   HAL::MapVertexFormat ( PrimitiveFillType fill, const VertexFormat* sourceFormat,
                              const VertexFormat** single,
                              const VertexFormat** batch, const VertexFormat** instanced, unsigned )
{
	return SManager.MapVertexFormat ( fill, sourceFormat, single, batch, instanced );
}

// Draws a range of pre-cached and preprocessed primitives
void HAL::DrawProcessedPrimitive ( Primitive* pprimitive, PrimitiveBatch* pstart, PrimitiveBatch *pend )
{
	SF_ASSERT ( pend != 0 );
	SF_ASSERT ( !Cache.AreBuffersLocked() );

	if ( !pprimitive->GetMeshCount() )
	{
		return;
	}

	// If in overdraw profile mode, and this primitive is part of a mask, draw it in color mode.
	static bool drawingMask = false;
	if ( !Profiler.ShouldDrawMask() && !drawingMask && ( HALState & HS_DrawingMask ) )
	{
		drawingMask = true;
		RHISetStencilState ( TStaticStencilState<FALSE>::GetRHI() );
		DrawProcessedPrimitive ( pprimitive, pstart, pend );
		drawingMask = false;
		RHISetStencilState ( CurStencilStateRHI );
	}

	PrimitiveBatch* pbatch = pstart ? pstart : pprimitive->Batches.GetFirst();

	if ( GEmitDrawEvents )
	{
		appBeginDrawEvent ( FColor ( 180, 0, 180 ), TEXT ( "GFxDrawProcessedPrimitive" ) );
	}

	unsigned bidx = 0;
	while ( pbatch != pend )
	{
		// pBatchMesh can be null in case of error, such as VB/IB lock failure.
		MeshCacheItem* pmesh = ( MeshCacheItem* ) pbatch->GetCacheItem();
		unsigned       meshIndex = pbatch->GetMeshIndex();
		unsigned       batchMeshCount = pbatch->GetMeshCount();

		if ( pmesh )
		{
			Profiler.SetBatch ( ( UPInt ) pprimitive, bidx );

			unsigned fillFlags = this->FillFlags;
			if ( pprimitive->GetMeshCount() && pprimitive->Meshes[0].M.Has3D() )
			{
				fillFlags |= FF_3DProjection;
			}

			ShaderPair pShader =
			    SManager.SetPrimitiveFill ( pprimitive->pFill, fillFlags, pbatch->Type, pbatch->pFormat, batchMeshCount, Matrices,
			                                &pprimitive->Meshes[meshIndex], &ShaderData );

			{
				check ( ( pbatch->Type != PrimitiveBatch::DP_Failed ) &&
				        ( pbatch->Type != PrimitiveBatch::DP_Virtual ) );

				// Draw the object with cached mesh.
				if ( pbatch->Type != PrimitiveBatch::DP_Instanced )
				{
					AccumulatedStats.Meshes += pmesh->MeshCount;
					AccumulatedStats.Triangles += pmesh->IndexCount / 3;
				}
				else
				{
					AccumulatedStats.Meshes += batchMeshCount;
					AccumulatedStats.Triangles += ( pmesh->IndexCount / 3 ) * batchMeshCount;
				}

				RHISetStreamSource ( 0, pmesh->GetVertexBuffer(), pbatch->pFormat->Size, pmesh->VertexOffset, FALSE, 0, 1 );
				RHIDrawIndexedPrimitive ( pmesh->GetIndexBuffer(), PT_TriangleList, 0, 0,
				                          pmesh->VertexCount, pmesh->IndexOffset / 2, pmesh->IndexCount / 3 );

				AccumulatedStats.Primitives++;
			}

#if RHI_UNIFIED_MEMORY
			pmesh->GPUFence = Cache.GetRenderSync()->InsertFence();
#endif
			pmesh->MoveToCacheListFront ( MCL_ThisFrame );
		}

		pbatch = pbatch->GetNext();
		bidx++;
	}

	if ( GEmitDrawEvents )
	{
		appEndDrawEvent();
	}

	// This assert shouldn't be here in the future since queue can have many items
	// SF_ASSERT(Cache.CachedItems[MCL_InFlight].IsEmpty());
}


void HAL::DrawProcessedComplexMeshes ( ComplexMesh* complexMesh,
                                       const StrideArray<HMatrix>& matrices )
{
	typedef ComplexMesh::FillRecord   FillRecord;
	typedef PrimitiveBatch::BatchType BatchType;

	// If in overdraw profile mode, and this primitive is part of a mask, draw it in color mode.
	static bool drawingMask = false;
	if ( !Profiler.ShouldDrawMask() && !drawingMask && ( HALState & HS_DrawingMask ) )
	{
		drawingMask = true;
		RHISetStencilState ( TStaticStencilState<FALSE>::GetRHI() );
		DrawProcessedComplexMeshes ( complexMesh, matrices );
		drawingMask = false;
		RHISetStencilState ( CurStencilStateRHI );
	}

	MeshCacheItem* pmesh = ( MeshCacheItem* ) complexMesh->GetCacheItem();
	if ( !pmesh )
	{
		return;
	}

	if ( GEmitDrawEvents )
	{
		appBeginDrawEvent ( FColor ( 180, 0, 180 ), TEXT ( "GFxDrawProcessedComplexMeshes" ) );
	}

	const FillRecord* fillRecords = complexMesh->GetFillRecords();
	unsigned    fillCount     = complexMesh->GetFillRecordCount();
	unsigned    instanceCount = ( unsigned ) matrices.GetSize();
	BatchType   batchType = PrimitiveBatch::DP_Single;

	const Matrix2F* textureMatrices = complexMesh->GetFillMatrixCache();

	UInt32 fillFlags = FillFlags | ( matrices[0].Has3D() ? FF_3DProjection : 0 );

	for ( unsigned fillIndex = 0; fillIndex < fillCount; fillIndex++ )
	{
		const FillRecord& fr = fillRecords[fillIndex];

		UByte textureCount = fr.pFill->GetTextureCount();
		unsigned startIndex = 0;
		unsigned fillFlags = FillFlags;

		if ( instanceCount > 0 )
		{
			const HMatrix& hm = matrices[0];
			fillFlags |= hm.Has3D() ? FF_3DProjection : 0;

			for ( unsigned i = 0; i < instanceCount; i++ )
			{
				const HMatrix& hm = matrices[startIndex + i];
				if ( ! ( Profiler.GetCxform ( hm.GetCxform() ) == Cxform::Identity ) )
				{
					fillFlags |= FF_Cxform;
				}
			}
		}

		// Apply fill.
		PrimitiveFillType fillType = Profiler.GetFillType ( fr.pFill->GetType() );
		ShaderPair pso = SManager.SetFill ( fillType, fillFlags, batchType, fr.pFormats[0], &ShaderData );

		unsigned VertexOffset = pmesh->VertexOffset + fr.VertexByteOffset;

		RHISetStreamSource ( 0, pmesh->GetVertexBuffer(), fr.pFormats[0]->Size, VertexOffset, FALSE, 0, 1 );

		bool solid = ( fillType == PrimFill_None || fillType == PrimFill_Mask || fillType == PrimFill_SolidColor );

		for ( unsigned i = 0; i < instanceCount; i++ )
		{
			const HMatrix& hm = matrices[startIndex + i];

			ShaderData.SetMatrix ( pso, Uniform::SU_mvp, complexMesh->GetVertexMatrix(), hm, Matrices );
			if ( solid )
			{
				ShaderData.SetColor ( pso, Uniform::SU_cxmul, Profiler.GetColor ( fr.pFill->GetSolidColor() ) );
			}
			else if ( fillFlags & FF_Cxform )
			{
				ShaderData.SetCxform ( pso, Profiler.GetCxform ( hm.GetCxform() ) );
			}

			for ( unsigned tm = 0, stage = 0; tm < textureCount; tm++ )
			{
				ShaderData.SetMatrix ( pso, Uniform::SU_texgen, textureMatrices[fr.FillMatrixIndex[tm]], tm );
				Texture* ptex = ( Texture* ) fr.pFill->GetTexture ( tm );
				ShaderData.SetTexture ( pso, stage, ptex, fr.pFill->GetFillMode ( tm ) );
				stage += ptex->GetTextureStageCount();
			}

			ShaderData.Finish ( 1 );

			RHIDrawIndexedPrimitive ( pmesh->GetIndexBuffer(), PT_TriangleList, 0, 0,
			                          fr.VertexCount, ( pmesh->IndexOffset / 2 ) + fr.IndexOffset, fr.IndexCount / 3 );
		}

		AccumulatedStats.Triangles += ( fr.IndexCount / 3 ) * instanceCount;
		AccumulatedStats.Meshes += instanceCount;
		AccumulatedStats.Primitives++;

	} // for (fill record)

#if RHI_UNIFIED_MEMORY
	pmesh->GPUFence = Cache.GetRenderSync()->InsertFence();
#endif
	pmesh->MoveToCacheListFront ( MCL_ThisFrame );

	if ( GEmitDrawEvents )
	{
		appEndDrawEvent();
	}
}

//--------------------------------------------------------------------
// Background clear helper, expects viewport coordinates.
void HAL::clearSolidRectangle ( const Rect<int>& r, Color color )
{
	if ( ! ( HALState & HS_ModeSet ) )
	{
		return;
	}

	color = Profiler.GetClearColor ( color );

	float colorf[4];
	color.GetRGBAFloat ( colorf, colorf + 1, colorf + 2, colorf + 3 );
	Matrix2F m ( ( float ) r.Width(), 0.0f, ( float ) r.x1,
	             0.0f, ( float ) r.Height(), ( float ) r.y1 );

	Matrix2F mvp ( m, Matrices.UserView );

	unsigned fillFlags = 0;
	ShaderPair pso = SManager.SetFill ( PrimFill_SolidColor, fillFlags, PrimitiveBatch::DP_Single, pVertexXY16IBatch, &ShaderData );
	ShaderData.SetMatrix ( pso, Uniform::SU_mvp, mvp );
	ShaderData.SetUniform ( pso, Uniform::SU_cxmul, colorf, 4 );
	ShaderData.Finish ( 1 );

	RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );
	RHIDrawPrimitive ( PT_TriangleList, 0, 2 );
}


//--------------------------------------------------------------------
// *** Mask / Stencil support
//--------------------------------------------------------------------

// Mask support is implemented as a stack, enabling for a number of optimizations:
//
// 1. Large "Clipped" marks are clipped to a custom viewport, allowing to save on
//    fill-rate when rendering both the mask and its content. The mask area threshold
//    that triggers this behavior is determined externally.
//      - Clipped masks can be nested, but not batched. When erased, clipped masks
//        clear the clipped intersection area.
// 2. Small masks can be Batched, having multiple mask areas with multiple mask
//    content items inside.
//      - Small masks can contain clipped masks either regular or clipped masks.
// 3. Mask area dimensions are provided as HMatrix, which maps a unit rectangle {0,0,1,1}
//    to a mask bounding rectangle. This rectangle can be rotated (non-axis aligned),
//    allowing for more efficient fill.
// 4. PopMask stack optimization is implemented that does not erase nested masks;
//    Stencil Reference value is changed instead. Erase of a mask only becomes
//    necessary if another PushMask_BeginSubmit is called, in which case previous
//    mask bounding rectangles are erased. This setup avoids often unnecessary erase
//    operations when drawing content following a nested mask.
//      - To implement this MaskStack keeps a previous "stale" MaskPrimitive
//        located above the MaskStackTop.

void HAL::PushMask_BeginSubmit ( MaskPrimitive* prim )
{
	if ( !checkState ( HS_InDisplay, "PushMask_BeginSubmit" ) )
	{
		return;
	}

	Profiler.SetDrawMode ( 1 );
	RHISetColorWriteEnable ( FALSE );

	bool viewportValid = ( HALState & HS_ViewValid ) != 0;

	// Erase previous mask if it existed above our current stack top.
	if ( MaskStackTop && ( MaskStack.GetSize() > MaskStackTop ) && viewportValid )
	{
		// Erase rectangles of these matrices; must be done even for clipped masks.
		// Any stencil of value greater then MaskStackTop should be set to it;
		// i.e. replace when (MaskStackTop < stencil value).

		FStencilStateInitializerRHI Initializer (
		    TRUE,      // bEnableFrontFaceStencil
		    CF_LessEqual,//FrontFaceStencilTest
		    SO_Keep,   // FrontFaceStencilFailStencilOp
		    SO_Keep,   // FrontFaceDepthFailStencilOp
		    SO_Replace,// FrontFacePassStencilOp
		    FALSE,     // bEnableBackFaceStencil
		    CF_Always, // BackFaceStencilTest
		    SO_Keep,   // BackFaceStencilFailStencilOp
		    SO_Keep,   // BackFaceDepthFailStencilOp
		    SO_Keep,   // BackFacePassStencilOp
		    0xFFFFFFFF,// StencilReadMask
		    0xFFFFFFFF,// StencilWriteMask
		    MaskStackTop//StencilRef
		);

		CurStencilStateRHI = RHICreateStencilState ( Initializer );
		RHISetStencilState ( CurStencilStateRHI );

		MaskPrimitive* erasePrim = MaskStack[MaskStackTop].pPrimitive;
		drawMaskClearRectangles ( erasePrim->GetMaskAreaMatrices(), erasePrim->GetMaskCount() );
	}

	MaskStack.Resize ( MaskStackTop + 1 );
	MaskStackEntry &e = MaskStack[MaskStackTop];
	e.pPrimitive       = prim;
	e.OldViewportValid = viewportValid;
	e.OldViewRect      = ViewRect; // TBD: Must assign
	MaskStackTop++;

	HALState |= HS_DrawingMask;

	if ( prim->IsClipped() && viewportValid )
	{
		Rect<int> boundClip;

		// Apply new viewport clipping.
		if ( !Matrices.OrientationSet )
		{
			const Matrix2F& m = prim->GetMaskAreaMatrix ( 0 ).GetMatrix2D();

			// Clipped matrices are always in View coordinate space, to allow
			// matrix to be use for erase operation above. This means that we don't
			// have to do an EncloseTransform.
			SF_ASSERT ( ( m.Shx() == 0.0f ) && ( m.Shy() == 0.0f ) );
			boundClip = Rect<int> ( VP.Left + ( int ) m.Tx(), VP.Top + ( int ) m.Ty(),
			                        VP.Left + ( int ) ( m.Tx() + m.Sx() ), VP.Top + ( int ) ( m.Ty() + m.Sy() ) );
		}
		else
		{
			Matrix2F m = prim->GetMaskAreaMatrix ( 0 ).GetMatrix2D();
			m.Append ( Matrices.Orient2D );

			RectF rect = m.EncloseTransform ( RectF ( 0, 0, 1, 1 ) );
			boundClip = Rect<int> ( VP.Left + ( int ) rect.x1, VP.Top + ( int ) rect.y1,
			                        VP.Left + ( int ) rect.x2, VP.Top + ( int ) rect.y2 );
		}

		if ( !ViewRect.IntersectRect ( &ViewRect, boundClip ) )
		{
			ViewRect.Clear();
			HALState &= ~HS_ViewValid;
			viewportValid = false;
		}
		updateViewport();

		// Clear full viewport area, which has been resized to our smaller bounds.
		if ( ( MaskStackTop == 1 ) && viewportValid )
		{
			RHIClear ( FALSE, FLinearColor::Black, // Color
			           FALSE, 0.0f,
			           TRUE,  0 );
		}
	}
	else if ( ( MaskStackTop == 1 ) && viewportValid )
	{
		// Clear view rectangles.
		RHISetStencilState ( MaskClearStencilStateRHI );

		drawMaskClearRectangles ( prim->GetMaskAreaMatrices(), prim->GetMaskCount() );
	}

	FStencilStateInitializerRHI Initializer (
	    TRUE,      // bEnableFrontFaceStencil
	    CF_Equal,  //FrontFaceStencilTest
	    SO_Keep,   // FrontFaceStencilFailStencilOp
	    SO_Keep,   // FrontFaceDepthFailStencilOp
	    SO_Increment,// FrontFacePassStencilOp
	    FALSE,     // bEnableBackFaceStencil
	    CF_Always, // BackFaceStencilTest
	    SO_Keep,   // BackFaceStencilFailStencilOp
	    SO_Keep,   // BackFaceDepthFailStencilOp
	    SO_Keep,   // BackFacePassStencilOp
	    0xFFFFFFFF,// StencilReadMask
	    0xFFFFFFFF,// StencilWriteMask
	    MaskStackTop - 1 //StencilRef
	);

	CurStencilStateRHI = RHICreateStencilState ( Initializer );
	RHISetStencilState ( CurStencilStateRHI );

	++AccumulatedStats.Masks;
}


void HAL::EndMaskSubmit()
{
	Profiler.SetDrawMode ( 0 );
	RHISetColorWriteEnable ( TRUE );

	if ( !checkState ( HS_InDisplay | HS_DrawingMask, "EndMaskSubmit" ) )
	{
		return;
	}
	HALState &= ~HS_DrawingMask;
	SF_ASSERT ( MaskStackTop );

	FStencilStateInitializerRHI Initializer (
	    TRUE,      // bEnableFrontFaceStencil
	    CF_LessEqual,//FrontFaceStencilTest
	    SO_Keep,   // FrontFaceStencilFailStencilOp
	    SO_Keep,   // FrontFaceDepthFailStencilOp
	    SO_Keep,   // FrontFacePassStencilOp
	    FALSE,     // bEnableBackFaceStencil
	    CF_Always, // BackFaceStencilTest
	    SO_Keep,   // BackFaceStencilFailStencilOp
	    SO_Keep,   // BackFaceDepthFailStencilOp
	    SO_Keep,   // BackFacePassStencilOp
	    0xFFFFFFFF,// StencilReadMask
	    0xFFFFFFFF,// StencilWriteMask
	    MaskStackTop//StencilRef
	);

	CurStencilStateRHI = RHICreateStencilState ( Initializer );
	RHISetStencilState ( CurStencilStateRHI );
}


void HAL::PopMask()
{
	if ( !checkState ( HS_InDisplay, "PopMask" ) )
	{
		return;
	}

	SF_ASSERT ( MaskStackTop );
	MaskStackTop--;

	if ( MaskStack[MaskStackTop].pPrimitive->IsClipped() )
	{
		// Restore viewport
		ViewRect = MaskStack[MaskStackTop].OldViewRect;

		if ( MaskStack[MaskStackTop].OldViewportValid )
		{
			HALState |= HS_ViewValid;
		}
		else
		{
			HALState &= ~HS_ViewValid;
		}
		updateViewport();
	}

	// Disable mask or decrement stencil reference value.
	if ( MaskStackTop == 0 )
	{
		CurStencilStateRHI = TStaticStencilState<FALSE>::GetRHI();
		RHISetStencilState ( CurStencilStateRHI );
	}
	else
	{
		// Change ref value down, so that we can draw using previous mask.
		FStencilStateInitializerRHI Initializer (
		    TRUE,      // bEnableFrontFaceStencil
		    CF_LessEqual,//FrontFaceStencilTest
		    SO_Keep,   // FrontFaceStencilFailStencilOp
		    SO_Keep,   // FrontFaceDepthFailStencilOp
		    SO_Keep,   // FrontFacePassStencilOp
		    FALSE,     // bEnableBackFaceStencil
		    CF_Always, // BackFaceStencilTest
		    SO_Keep,   // BackFaceStencilFailStencilOp
		    SO_Keep,   // BackFaceDepthFailStencilOp
		    SO_Keep,   // BackFacePassStencilOp
		    0xFFFFFFFF,// StencilReadMask
		    0xFFFFFFFF,// StencilWriteMask
		    MaskStackTop//StencilRef
		);

		CurStencilStateRHI = RHICreateStencilState ( Initializer );
		RHISetStencilState ( CurStencilStateRHI );
	}
}

void HAL::drawMaskClearRectangles ( const HMatrix* matrices, UPInt count )
{
	// This operation is used to clear bounds for masks.
	// Potential issue: Since our bounds are exact, right/bottom pixels may not
	// be drawn due to HW fill rules.
	//  - This shouldn't matter if a mask is tessellated within bounds of those
	//    coordinates, since same rules are applied to those render shapes.
	//  - EdgeAA must be turned off for masks, as that would extrude the bounds.

	unsigned fillFlags = 0;
	ShaderPair pso = SManager.SetFill ( PrimFill_SolidColor, fillFlags, PrimitiveBatch::DP_Batch, pVertexXY16IBatch, &ShaderData );

	RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );

	const float colorf[] = {1, 0, 0, 0.5f};

	// Draw the object with cached mesh.
	unsigned drawRangeCount = 0;
	for ( UPInt i = 0; i < count; i += ( UPInt ) drawRangeCount )
	{
		drawRangeCount = Alg::Min<unsigned> ( ( unsigned ) count, MeshCache::MaxEraseBatchCount );

		for ( unsigned j = 0; j < drawRangeCount; j++ )
		{
			ShaderData.SetMatrix ( pso, Uniform::SU_mvp, Matrix2F::Identity, matrices[i + j], Matrices, 0, j );
			ShaderData.SetUniform ( pso, Uniform::SU_cxmul, colorf, 4 );
		}
		ShaderData.Finish ( drawRangeCount );

		RHIDrawPrimitive ( PT_TriangleList, 0, drawRangeCount * 2 );

		AccumulatedStats.Meshes += drawRangeCount;
		AccumulatedStats.Triangles += drawRangeCount * 2;
		AccumulatedStats.Primitives++;
	}
}


//--------------------------------------------------------------------
// *** BlendMode Stack support
//--------------------------------------------------------------------

// Structure describing color combines applied for a given blend mode.

void HAL::applyBlendMode ( BlendMode NewMode, bool bSourceAc, bool bForceAc )
{
	// For debug build
	SF_ASSERT ( ( ( unsigned ) NewMode ) < 15 );
	// For release
	if ( ( ( unsigned ) NewMode ) >= 15 )
	{
		NewMode = Blend_None;
	}

	NewMode = Profiler.GetBlendMode ( NewMode );

	// Multiply requires different fill mode, save it in the HAL's fill flags.
	if ( NewMode == Blend_Multiply || NewMode == Blend_Darken )
	{
		FillFlags |= FF_Multiply;
	}
	else
	{
		FillFlags &= ~FF_Multiply;
	}

#if NGP
    INT BlendMode;
    switch ( NewMode )
    {
    case Blend_Multiply:
        BlendMode = EGFXBM_Multiply;
        break;
    case Blend_Lighten:
        BlendMode = EGFXBM_Lighten;
        break;
    case Blend_Darken:
        BlendMode = EGFXBM_Darken;
        break;
    case Blend_Add:
        BlendMode = EGFXBM_Add;
        break;
    case Blend_Subtract:
        BlendMode = EGFXBM_Subtract;
        break;
    default:
        BlendMode = EGFXBM_Normal;
        break;
    }

    if ( bSourceAc )
    {
        BlendMode |= EGFXBM_SourceAc;
    }
    else if ( ( VP.Flags & Viewport::View_AlphaComposite ) || bForceAc )
    {
        BlendMode |= EGFXBM_DestAc;
    }

    RHISetMobileGFxParams ( (EMobileGFxBlendMode) BlendMode );

#else
	if ( bSourceAc )
	{
		switch ( NewMode )
		{
			case Blend_None:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Normal:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Layer:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Multiply:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_DestColor,   BF_Zero,
				                   BO_Add, BF_DestAlpha,   BF_Zero >::GetRHI() );
				break;
			case Blend_Screen:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break; //???
			case Blend_Lighten:
				RHISetBlendState ( TStaticBlendState < BO_Max, BF_One,         BF_One,
				                   BO_Max, BF_One,         BF_One >::GetRHI() );
				break;
			case Blend_Darken:
				RHISetBlendState ( TStaticBlendState < BO_Min, BF_One,         BF_One,
				                   BO_Min, BF_One,         BF_One >::GetRHI() );
				break;
			case Blend_Difference:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Add:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_One,
				                   BO_Add, BF_Zero,        BF_One >::GetRHI() );
				break;
			case Blend_Subtract:
				RHISetBlendState ( TStaticBlendState < BO_ReverseSubtract, BF_One,         BF_One,
				                   BO_ReverseSubtract, BF_Zero,        BF_One >::GetRHI() );
				break;
			case Blend_Invert:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Alpha:
				RHISetBlendState ( TStaticBlendState<BO_Add, BF_Zero, BF_One>::GetRHI() );
				break; //???
			case Blend_Erase:
				RHISetBlendState ( TStaticBlendState<BO_Add, BF_Zero, BF_One>::GetRHI() );
				break; //???
			case Blend_Overlay:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_HardLight:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One,         BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			default:
				checkMsg ( 0, "ApplyBlendMode:  Invalid BlendType ApplyBlendMode encountered by FGFxRenderer" );
				break;
		}
	}
	else if ( ( VP.Flags & Viewport::View_AlphaComposite ) || bForceAc )
	{
		switch ( NewMode )
		{
			case Blend_None:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Normal:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Layer:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Multiply:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_DestColor,   BF_Zero,
				                   BO_Add, BF_DestAlpha,   BF_Zero >::GetRHI() );
				break;
			case Blend_Screen:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break; //???
			case Blend_Lighten:
				RHISetBlendState ( TStaticBlendState < BO_Max, BF_SourceAlpha, BF_One,
				                   BO_Max, BF_One,         BF_One >::GetRHI() );
				break;
			case Blend_Darken:
				RHISetBlendState ( TStaticBlendState < BO_Min, BF_SourceAlpha, BF_One,
				                   BO_Min, BF_One,         BF_One >::GetRHI() );
				break;
			case Blend_Difference:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Add:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_One,
				                   BO_Add, BF_Zero,        BF_One >::GetRHI() );
				break;
			case Blend_Subtract:
				RHISetBlendState ( TStaticBlendState < BO_ReverseSubtract, BF_SourceAlpha, BF_One,
				                   BO_ReverseSubtract, BF_Zero,        BF_One >::GetRHI() );
				break;
			case Blend_Invert:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Alpha:
				RHISetBlendState ( TStaticBlendState<BO_Add, BF_Zero, BF_One>::GetRHI() );
				break; //???
			case Blend_Erase:
				RHISetBlendState ( TStaticBlendState<BO_Add, BF_Zero, BF_One>::GetRHI() );
				break; //???
			case Blend_Overlay:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_One, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_HardLight:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_One,         BF_InverseSourceAlpha >::GetRHI() );
				break;
			default:
				checkMsg ( 0, "ApplyBlendMode:  Invalid BlendType ApplyBlendMode encountered by FGFxRenderer" );
				break;
		}
	}
	else
		switch ( NewMode )
		{
			case Blend_None:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Normal:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Layer:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break;
			case Blend_Multiply:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_DestColor, BF_Zero,
				                   BO_Add, BF_DestAlpha, BF_Zero >::GetRHI() );
				break;
			case Blend_Screen:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break; //???
			case Blend_Lighten:
				RHISetBlendState ( TStaticBlendState < BO_Max, BF_SourceAlpha, BF_One,
				                   BO_Max, BF_SourceAlpha, BF_One >::GetRHI() );
				break;
			case Blend_Darken:
				RHISetBlendState ( TStaticBlendState < BO_Min, BF_SourceAlpha, BF_One,
				                   BO_Min, BF_SourceAlpha, BF_One >::GetRHI() );
				break;
			case Blend_Difference:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break; //???
			case Blend_Add:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_One,
				                   BO_Add, BF_SourceAlpha, BF_One >::GetRHI() );
				break;
			case Blend_Subtract:
				RHISetBlendState ( TStaticBlendState < BO_ReverseSubtract, BF_SourceAlpha, BF_One,
				                   BO_ReverseSubtract, BF_SourceAlpha, BF_One >::GetRHI() );
				break;
			case Blend_Invert:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break; //???
			case Blend_Alpha:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_Zero, BF_One,
				                   BO_Add, BF_Zero, BF_One >::GetRHI() );
				break; //???
			case Blend_Erase:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_Zero, BF_One,
				                   BO_Add, BF_Zero, BF_One >::GetRHI() );
				break; //???
			case Blend_Overlay:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break; //???
			case Blend_HardLight:
				RHISetBlendState ( TStaticBlendState < BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
				                   BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
				break; //???
		}

#if WITH_MOBILE_RHI
    // prevent unnecessary shader compiling
    RHISetMobileSimpleParams ( BLEND_Opaque );
#endif

#endif
}

#if !XBOX

void HAL::PushFilters ( FilterPrimitive* prim )
{
	if ( !checkState ( HS_InDisplay, __FUNCTION__ ) )
	{
		return;
	}

	FilterStackEntry e = {prim, 0};

	// Queue the profiler off of whether masks should be draw or not.
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

	if ( ( HALState & HS_CachedFilter ) )
	{
		FilterStack.PushBack ( e );
		return;
	}

	// Disable masking from previous target, if this filter primitive doesn't have any masking.
	if ( MaskStackTop != 0 && !prim->GetMaskPresent() )
	{
		RHISetColorWriteEnable ( TRUE );
		RHISetStencilState ( TStaticStencilState<FALSE>::GetRHI() );
	}

	HALState |= HS_DrawingFilter;

	if ( prim->GetCacheState() ==  FilterPrimitive::Cache_Uncached )
	{
		// Draw the filter from scratch.
		const Matrix2F& m = e.pPrimitive->GetFilterAreaMatrix().GetMatrix2D();
		e.pRenderTarget = *CreateTempRenderTarget ( ImageSize ( ( UInt32 ) m.Sx(), ( UInt32 ) m.Sy() ), prim->GetMaskPresent() );
		RectF frameRect ( m.Tx(), m.Ty(), m.Tx() + m.Sx(), m.Ty() + m.Sy() );
		PushRenderTarget ( frameRect, e.pRenderTarget );
		applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, false, true );

		// If this primitive has masking, then clear the entire area to the current mask level, because
		// the depth stencil target may be different, and thus does not contain the previously written values.
		if ( prim->GetMaskPresent() )
		{
#if NGP
			// PushRenderTarget did this
#else
			RHIClear ( FALSE, FLinearColor::Black, FALSE, 0, TRUE, MaskStackTop );
#endif
		}
	}
	else
	{
		// Drawing a cached filter, ignore all draw calls until the corresponding PopFilters.
		// Keep track of the level at which we need to draw the cached filter, by adding entries to the stack.
		HALState |= HS_CachedFilter;
		CachedFilterIndex = FilterStack.GetSize();
		QueueProcessor.SetQueueEmitFilter ( RenderQueueProcessor::QPF_Filters );
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
		drawCachedFilter ( e.pPrimitive );
		QueueProcessor.SetQueueEmitFilter ( RenderQueueProcessor::QPF_All );
		HALState &= ~HS_CachedFilter;
	}
	else
	{
		drawUncachedFilter ( e );
	}

	if ( FilterStack.GetSize() == 0 )
	{
		HALState &= ~HS_DrawingFilter;
	}
}

void HAL::drawUncachedFilter ( const FilterStackEntry& e )
{
	const FilterSet* filters = e.pPrimitive->GetFilters();
	unsigned filterCount = filters->GetFilterCount();
	const Filter* filter = 0;
	unsigned pass = 0, passes = 0;

	// Invalid primitive or rendertarget.
	if ( !e.pPrimitive || !e.pRenderTarget )
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
	temporaryTextures[0] = e.pRenderTarget;

	RenderTargetData* RTData0 = ( RenderTargetData* ) temporaryTextures[0]->GetHALData();
	RHICopyToResolveTarget ( RTData0->Resource.ColorBuffer, FALSE, FResolveParams() );

	RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );

	RHISetColorWriteEnable ( TRUE );
	// Overlay mode isn't actually supported, it contains the blend mode for filter sub-targets.
	applyBlendMode ( Blend_Overlay, true, false );

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
			// Render the final pass directly to the target surface.
			if ( pass == passes - 1 && i == filterCount - 1 )
			{
				break;
			}

			// Create a destination texture if required.
			if ( !temporaryTextures[1] )
			{
				temporaryTextures[1] = *CreateTempRenderTarget ( size, false );
			}

			RHISetRenderTarget ( ( ( RenderTargetData* ) temporaryTextures[1]->GetHALData() )->Resource.ColorBuffer, 0 );
            RenderTarget* prt = temporaryTextures[1];
            const Rect<int>& viewRect = prt->GetRect(); // On the render texture, might not be the entire surface.
            const ImageSize& bs = prt->GetBufferSize();
#if NGP
			RHISetMobileGFxParams( EGFXBM_None );
            clearSolidRectangle(Rect<int>(bs.Width*40, bs.Height*40), 0);
			RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );
			applyBlendMode( Blend_Overlay, true, false );
#else
			RHIClear ( TRUE, FColor ( 0 ), FALSE, 0.0f, FALSE, 0 );
#endif
			++AccumulatedStats.RTChanges;

			// Scale to the size of the destination.
			VP = Viewport ( bs.Width, bs.Height, viewRect.x1, viewRect.y1, viewRect.Width(), viewRect.Height() );
			ViewRect = Rect<int> ( viewRect.x1, viewRect.y1, viewRect.x2, viewRect.y2 );
			HALState |= HS_ViewValid;
			updateViewport();

#if _WINDOWS || XBOX
            Matrix2F mvp;
            if (GRHIShaderPlatform == SP_PCOGL)
            {
                mvp = Matrix2F::Scaling ( 2, 2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
            }
            else
            {
			    mvp = Matrix2F::Scaling ( 2, -2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
    			mvp.Tx() -= 2 * GPixelCenterOffset / size.Width;
	    		mvp.Ty() += 2 * GPixelCenterOffset / size.Height;
            }
#elif PLATFORM_DESKTOP || MOBILE && !NGP
            Matrix2F mvp = Matrix2F::Scaling ( 2, 2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
#else
            Matrix2F mvp = Matrix2F::Scaling ( 2, -2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
#endif
			SManager.SetFilterFill ( mvp, Cxform::Identity, filter, temporaryTextures, shaders, pass, passes, pVertexXY16IBatch, &ShaderData );

			RHIDrawPrimitive ( PT_TriangleList, 0, 2 );

			// If we require the original source, save it, and create an additional destination target.
			if ( requireSource && pass == 0 )
			{
				temporaryTextures[2] = temporaryTextures[0];
				temporaryTextures[0] = *CreateTempRenderTarget ( size, false );
			}

			RenderTargetData* RTData1 = ( RenderTargetData* ) temporaryTextures[Target_Destination]->GetHALData();
			RHICopyToResolveTarget( RTData1->Resource.ColorBuffer, FALSE, FResolveParams() );

			// Setup for the next pass.
			Alg::Swap ( temporaryTextures[0], temporaryTextures[1] );
		}

		AccumulatedStats.Primitives += passes;
		AccumulatedStats.Meshes     += passes;
	}

	// Cache the 2nd last step so it might be available as a cached filter next time.
	if (Profiler.IsFilterCachingEnabled() && temporaryTextures[Target_Source])
	{
		RenderTarget* cacheResults[2] = { temporaryTextures[0], temporaryTextures[2] };
		e.pPrimitive->SetCacheResults ( FilterPrimitive::Cache_PreTarget, cacheResults, 2 );
		( ( RenderTargetData* ) cacheResults[0]->GetHALData() )->CacheID = reinterpret_cast<UPInt> ( e.pPrimitive.GetPtr() );
		if ( cacheResults[1] )
		{
			( ( RenderTargetData* ) cacheResults[1]->GetHALData() )->CacheID = reinterpret_cast<UPInt> ( e.pPrimitive.GetPtr() );
		}
	}

	// Pop the temporary target, begin rendering to the previous surface.
	PopRenderTarget();

	// Re-[en/dis]able masking from previous target, if available.
	if ( MaskStackTop != 0 )
	{
		RHISetStencilState ( CurStencilStateRHI );
	}

	applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, true, true );

	// Now actually draw the filtered sub-scene to the target below.
	const Matrix2F& mvp = Matrices.UserView * e.pPrimitive->GetFilterAreaMatrix().GetMatrix2D();
	SManager.SetFilterFill ( mvp,  e.pPrimitive->GetFilterAreaMatrix().GetCxform(),
	                         filter, temporaryTextures, shaders, pass, passes, pVertexXY16IBatch, &ShaderData );
#if NGP || WIIU
	// should not be necessary, but is.
	RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );
#endif
	RHIDrawPrimitive ( PT_TriangleList, 0, 2 );
	applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, false, ( HALState & HS_InRenderTarget ) != 0 );

	// Re-[en/dis]able masking from previous target, if available.
	if ( MaskStackTop != 0 )
	{
		RHISetColorWriteEnable ( FALSE );
	}

	// Cleanup.
	for ( int i = 0; i < MaxTemporaryTextures; ++i )
	{
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

#endif // !XBOX

void HAL::drawCachedFilter ( FilterPrimitive* primitive )
{
	RHISetStreamSource ( 0, Cache.MaskEraseBatchVertexBuffer, pVertexXY16IBatch->Size, 0, FALSE, 0, 0 );

	const int MaxTemporaryTextures = 3;
	switch ( primitive->GetCacheState() )
	{
			// We have one-step from final target. Render it to a final target now.
		case FilterPrimitive::Cache_PreTarget:
		{
			if ( GEmitDrawEvents )
			{
				appBeginDrawEvent ( FColor ( 180, 0, 180 ), TEXT ( "GFxDrawCachedPreFilter" ) );
			}

			const FilterSet* filters = primitive->GetFilters();
			UPInt filterIndex = filters->GetFilterCount() - 1;
			const Filter* filter = filters->GetFilter ( filterIndex );
			unsigned shaders[ShaderManager::MaximumFilterPasses];
			unsigned passes = SManager.GetFilterPasses ( filter, FillFlags, shaders );

			// Fill out the temporary textures from the cached results.
			Ptr<RenderTarget> temporaryTextures[MaxTemporaryTextures];
			appMemset ( temporaryTextures, 0, sizeof temporaryTextures );
			RenderTarget* results[2];
			primitive->GetCacheResults ( results, 2 );
			temporaryTextures[0] = results[0];
			ImageSize size = temporaryTextures[0]->GetSize();
			temporaryTextures[1] = *CreateTempRenderTarget ( size, false );
			temporaryTextures[2] = results[1];
			PushRenderTarget ( RectF ( ( float ) size.Width, ( float ) size.Height ), temporaryTextures[1] );

			// Render to the target.
#if _WINDOWS || XBOX
            Matrix2F mvp;
            if (GRHIShaderPlatform == SP_PCOGL)
            {
                mvp = Matrix2F::Scaling ( 2, 2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
            }
            else
            {
                mvp = Matrix2F::Scaling ( 2, -2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
                mvp.Tx() -= 2 * GPixelCenterOffset / size.Width;
                mvp.Ty() += 2 * GPixelCenterOffset / size.Height;
            }
#elif PLATFORM_DESKTOP || MOBILE && !NGP
            Matrix2F mvp = Matrix2F::Scaling ( 2, 2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
#else
            Matrix2F mvp = Matrix2F::Scaling ( 2, -2 ) * Matrix2F::Translation ( -0.5f, -0.5f );
#endif

			const Cxform & cx = primitive->GetFilterAreaMatrix().GetCxform();
			SManager.SetFilterFill ( mvp, cx, filter, temporaryTextures, shaders, passes - 1, passes, pVertexXY16IBatch, &ShaderData );
			applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, true, true );
			RHIDrawPrimitive ( PT_TriangleList, 0, 2 );
			PopRenderTarget();

			// Set this as the final cache result, and then render it.
			RenderTarget* prt = temporaryTextures[1];
			primitive->SetCacheResults ( FilterPrimitive::Cache_Target, &prt, 1 );
			( ( RenderTargetData* ) prt->GetHALData() )->CacheID = reinterpret_cast<UPInt> ( primitive );
			drawCachedFilter ( primitive );

			// Cleanup.
			for ( int i = 0; i < MaxTemporaryTextures; ++i )
			{
				if ( temporaryTextures[i] )
				{
					temporaryTextures[i]->SetInUse ( false );
				}
			}
			break;
		}

		// We have a final filtered texture. Just apply it to a screen quad.
		case FilterPrimitive::Cache_Target:
		{
			if ( GEmitDrawEvents )
			{
				appBeginDrawEvent ( FColor ( 180, 0, 180 ), TEXT ( "GFxDrawCachedFilter" ) );
			}

#if XBOX
			unsigned fillFlags = (FillFlags & FF_Multiply) | (FF_AlphaWrite|FF_Cxform);
#else
			unsigned fillFlags = FillFlags & FF_Multiply;
#endif
			const ShaderManager::Shader& pso = SManager.SetFill (
			                                       PrimFill_Texture, fillFlags, PrimitiveBatch::DP_Single, pVertexXY16IBatch, &ShaderData );

			RenderTarget* results;
			primitive->GetCacheResults ( &results, 1 );
			Texture* ptexture = ( Texture* ) results->GetTexture();
			const Matrix2F& mvp = Matrices.UserView * primitive->GetFilterAreaMatrix().GetMatrix2D();
			const Rect<int>& srect = results->GetRect();
			Matrix2F texgen;
			if ( ptexture )
			{
				texgen.AppendTranslation ( ( float ) srect.x1, ( float ) srect.y1 );
				texgen.AppendScaling ( ( float ) srect.Width() / ptexture->GetSize().Width,
				                       ( float ) srect.Height() / ptexture->GetSize().Height );
				ShaderData.SetUniform ( pso, Uniform::SU_texgen, &texgen.M[0][0], 8 );
				ShaderData.SetTexture ( pso, 0, ptexture, ImageFillMode ( Wrap_Clamp, Sample_Linear ) );
			}
            if (fillFlags & FF_Cxform)
            {
                const Cxform & cx = primitive->GetFilterAreaMatrix().GetCxform();
                ShaderData.SetCxform( pso, cx );
            }
			ShaderData.SetUniform ( pso, Uniform::SU_mvp, &mvp.M[0][0], 8 );
			ShaderData.Finish ( 0 );

			applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, true, true );
			RHIDrawPrimitive ( PT_TriangleList, 0, 2 );
			applyBlendMode ( BlendModeStack.GetSize() >= 1 ? BlendModeStack.Back() : Blend_Normal, false, ( HALState & HS_InRenderTarget ) != 0 );

			// Cleanup.
			results->SetInUse ( false );
			if ( !Profiler.IsFilterCachingEnabled() )
			{
				primitive->SetCacheResults ( FilterPrimitive::Cache_Uncached, 0, 0 );
			}
			break;
		}

		// Should have been one of the other two caching types.
		default:
			SF_ASSERT ( 0 );
			return;
	}

	if ( GEmitDrawEvents )
	{
		appEndDrawEvent();
	}
}



void RenderTargetData::UpdateData ( RenderBuffer* InBuffer, FRenderTarget* InOwner, Texture* InTexture, DepthStencilBuffer* InDepthBuffer, DepthStencilSurface* InDepthSurface )
{
	if ( !InBuffer )
	{
		return;
	}

	RenderTargetData* poldHD = ( RenderTargetData* ) InBuffer->GetHALData();
	if ( !poldHD )
	{
		if ( InOwner )
		{
			poldHD = SF_NEW RenderTargetData ( InBuffer, InOwner, InDepthBuffer, InDepthSurface );
		}
		else
		{
			poldHD = SF_NEW RenderTargetData ( InBuffer, InTexture, InDepthBuffer, InDepthSurface );
		}
		InBuffer->SetHALData ( poldHD );
		return;
	}

	poldHD->Resource.ReleaseResource();

	poldHD->Resource.Size = ImageSize ( InOwner->GetSizeX(), InOwner->GetSizeY() );
	poldHD->Resource.InverseGamma = 1.0f;
	poldHD->Resource.Owner = InOwner;
	poldHD->Resource.Texture = InTexture;
	poldHD->Resource.InitResource();

	poldHD->pDepthStencilBuffer = InDepthBuffer;
	if ( InDepthSurface )
	{
		poldHD->DepthStencil = InDepthSurface;
	}
	else if ( InDepthBuffer )
	{
		poldHD->DepthStencil = ( DepthStencilSurface* ) InDepthBuffer->GetSurface();
	}
	else
	{
		poldHD->DepthStencil = 0;
	}
	poldHD->CacheID          = 0;
}

RenderTargetData::RenderTargetData ( RenderBuffer* InBuffer, FRenderTarget* InOwner,
                                     DepthStencilBuffer* InDepthBuffer, DepthStencilSurface* InDepthSurface ) :
	RenderBuffer::HALData ( InBuffer, InDepthBuffer )
	,   Resource ( InOwner, ImageSize ( InOwner->GetSizeX(), InOwner->GetSizeY() ) )
	,   DepthStencil ( NULL )
{
	if ( InDepthSurface )
	{
		DepthStencil = InDepthSurface;
	}
	else if ( InDepthBuffer )
	{
		DepthStencil = ( RHI::DepthStencilSurface* ) InDepthBuffer->GetSurface();
	}
}

RenderTargetData::RenderTargetData ( RenderBuffer* InBuffer, Texture * InTexture, DepthStencilBuffer* InDepthBuffer, DepthStencilSurface* InDepthSurface )
	:   RenderBuffer::HALData ( InBuffer, InDepthBuffer )
	,   Resource ( InTexture, InTexture->GetSize() )
	,   DepthStencil ( NULL )
{
	if ( InDepthSurface )
	{
		DepthStencil = InDepthSurface;
	}
	else if ( InDepthBuffer )
	{
		DepthStencil = ( RHI::DepthStencilSurface* ) InDepthBuffer->GetSurface();
	}
}


RenderTargetData::~RenderTargetData()
{
	check ( IsInRenderingThread() );
	Resource.ReleaseResource();
}

}
}
} // Scaleform::Render::RHI


#endif//WITH_GFx
