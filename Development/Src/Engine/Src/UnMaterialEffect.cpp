/*=============================================================================
	UMaterialEffect.cpp: Material post process effect implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "TileRendering.h"
#include "EngineMaterialClasses.h"

IMPLEMENT_CLASS(UMaterialEffect);

/*-----------------------------------------------------------------------------
FMaterialPostProcessSceneProxy
-----------------------------------------------------------------------------*/

/**
* Render-side data and rendering functionality for a UMaterialEffect
*/
class FMaterialPostProcessSceneProxy : public FPostProcessSceneProxy
{
public:
	/** 
	 * Initialization constructor. 
	 * @param InEffect - material post process effect to mirror in this proxy
	 */
	FMaterialPostProcessSceneProxy(const UMaterialEffect* InEffect,const FPostProcessSettings* WorldSettings)
	:	FPostProcessSceneProxy(InEffect)
	{
		UMaterial* BaseMaterial = InEffect->Material ? InEffect->Material->GetMaterial() : NULL;
		UMaterialInterface* EffectiveMaterial = InEffect->Material;
		
		if(EffectiveMaterial && !EffectiveMaterial->CheckMaterialUsage(MATUSAGE_MaterialEffect))
		{
			EffectiveMaterial = NULL;
		}
		
		// Check whether applied material is not unlit as it's a waste of GPU cycles.
		if( BaseMaterial && BaseMaterial->LightingModel != MLM_Unlit )
		{
			UBOOL bFixUpMaterial = FALSE;
		
			if( GIsEditor )
			{
				// Prompt user whether to fix up the material under the hood.
				bFixUpMaterial = appMsgf( AMT_YesNo, TEXT("%s assigned to %s via %s uses lighting mode other than MLM_Unlit. Do you want this changed?"), 
									*BaseMaterial->GetFullName(),
									*InEffect->GetFullName(),
									*BaseMaterial->GetFullName() );
				// Fix up lighting model, recompile and mark package dirty.
				if( bFixUpMaterial )
				{
					BaseMaterial->LightingModel = MLM_Unlit;
					BaseMaterial->PostEditChange();
					BaseMaterial->MarkPackageDirty();
				}
			}

			// BaseMaterial has been fixed up, use it.
			if( bFixUpMaterial && EffectiveMaterial )
			{
				MaterialRenderProxy = EffectiveMaterial->GetRenderProxy(FALSE);
			}
			// BaseMaterial hasn't been fixed up, use default emissive material and log warning.
			else
			{
				warnf(TEXT("%s assigned to %s via %s uses lighting mode other than MLM_Unlit."),
					*BaseMaterial->GetFullName(),
					*InEffect->GetFullName(),
					*InEffect->Material->GetFullName() );
				MaterialRenderProxy = GEngine->EmissiveTexturedMaterial->GetRenderProxy(FALSE);
			}
		}
		else
		{
			MaterialRenderProxy = EffectiveMaterial ? EffectiveMaterial->GetRenderProxy(FALSE) : GEngine->EmissiveTexturedMaterial->GetRenderProxy(FALSE);
		}
	}

	/**
	 * Render the post process effect
	 * Called by the rendering thread during scene rendering
	 * @param InDepthPriorityGroup - scene DPG currently being rendered
	 * @param View - current view
	 * @param CanvasTransform - same canvas transform used to render the scene
	 * @param LDRInfo - helper information about SceneColorLDR
	 * @return TRUE if anything was rendered
	 */
	UBOOL Render(const FScene* Scene, UINT InDepthPriorityGroup,FViewInfo& View,const FMatrix& CanvasTransform,FSceneColorLDRInfo& LDRInfo)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("MaterialEffect"));

		UBOOL bDirty=TRUE;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial();
		check(Material);

		// distortion materials are not handled
		if( Material->IsDistorted() )
		{
			bDirty = FALSE;
			return bDirty;
		}
		if (View.bUseLDRSceneColor) // Using 32-bit (LDR) surface
		{
			// Normally, we're going to ping-pong SceneColorLDR between the surface and its resolve-target.
			// But if we're drawing to secondary views (e.g. splitscreen), we must make sure that the final result is drawn to the same
			// memory as view0, so that it will contain the final fullscreen image after all postprocess.
			// To do this, we need to detect if there are an odd number of effects that draw to LDR (on the secondary view), since an
			// even number of ping-pongs will place the final image in the same buffer as view0.
			// On PS3 SceneColorLDR, LightAttenuation and BackBuffer refers to the same memory.
			DWORD UsageFlags = RTUsage_FullOverwrite;
			if ( LDRInfo.bAdjustPingPong && (LDRInfo.NumPingPongsRemaining & 1) )
			{
				UsageFlags |= RTUsage_DontSwapBuffer;
			}

			// If this is the final effect in chain, render to the view's output render target
			// unless an upscale is needed, in which case render to LDR scene color.
			if (FinalEffectInGroup && !GSystemSettings.NeedsUpscale()) 
			{
				// Render to the final render target
				GSceneRenderTargets.BeginRenderingBackBuffer( UsageFlags );

				// viewport to match view size 
				RHISetViewport(appTrunc(View.X),appTrunc(View.Y),0.0f,appTrunc(View.X + View.SizeX),appTrunc(View.Y + View.SizeY),1.0f);
			}
			else
			{
				GSceneRenderTargets.BeginRenderingSceneColorLDR( UsageFlags );

				// viewport to match render target size
				RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
			}
		}
		else // Using 64-bit (HDR) surface
		{
			GSceneRenderTargets.BeginRenderingSceneColor( RTUsage_FullOverwrite ); // Start rendering to the scene color buffer.

			// viewport to match view size
			RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
		}

		// disable depth test & writes
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());	

		// Turn off alpha-writes, since we sometimes store the Z-values there.
		RHISetColorWriteMask( CW_RGB);		

		FTileRenderer TileRenderer;
		// draw a full-view tile (the TileRenderer uses SizeX, not RenderTargetSizeX)
		//check(View.SizeX == View.RenderTargetSizeX);
		TileRenderer.DrawTile( View, MaterialRenderProxy);

		// Turn on alpha-writes again.
		RHISetColorWriteMask( CW_RGBA);

		// Resolve the scene color buffer.  Note that nothing needs to be done if we are writing directly to the view's render target
		if (View.bUseLDRSceneColor)
		{
			if (!FinalEffectInGroup || GSystemSettings.NeedsUpscale())
			{
				GSceneRenderTargets.FinishRenderingSceneColorLDR();
			}
		}
		else
		{
			GSceneRenderTargets.FinishRenderingSceneColor();
		}

		return bDirty;
	}

	/**
	 * Whether the effect may potentially render to SceneColorLDR or not.
	 * @return TRUE if the effect may potentially render to SceneColorLDR, otherwise FALSE.
	 */
	virtual UBOOL MayRenderSceneColorLDR() const
	{
		return TRUE;
	}

private:
	/** Material instance used by the effect */
	const FMaterialRenderProxy* MaterialRenderProxy;
};

/*-----------------------------------------------------------------------------
UMaterialEffect
-----------------------------------------------------------------------------*/

/**
* Creates a proxy to represent the render info for a post process effect
* @return The proxy object.
*/
FPostProcessSceneProxy* UMaterialEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	if (Material)
	{
		return new FMaterialPostProcessSceneProxy(this,WorldSettings);
	}
	else
	{
		return NULL;
	}
}
