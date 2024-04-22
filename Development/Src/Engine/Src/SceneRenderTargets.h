/*=============================================================================
	SceneRenderTargets.h: Scene render target definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCENERENDERTARGETS_H__
#define __SCENERENDERTARGETS_H__

#include "UnSkeletalRenderGPUSkin.h"	// FPreviousPerBoneMotionBlur

/**
 *	Bit-flags to indicate how a rendertarget is about to be used.
 **/
enum FSceneRenderTargetUsage
{
	RTUsage_Default			= 0x0000,		// For normal rendering
	RTUsage_FullOverwrite	= 0x0001,		// (PS3) Makes sure surface and resolvetarget points to different memory. Use this if the next drawcall is a fullscreen quad.
	RTUsage_DontSwapBuffer	= 0x0002,		// (PS3) Overrides the swapping the surface and resolvetarget. (Don't ping-pong fullscreen drawcalls.)
	RTUsage_RestoreSurface	= 0x0004,		// If set then copies the contents of the rendertarget Texture to its Surface
	RTUsage_ResolveDepth	= 0x0008,		// Whether the resulting depth values needs to be resolved at the end of this pass
	RTUsage_DWORD			= 0x7fffffff
};

enum ESceneRenderTargetTypes
{
	// Render target for post process filter colors.
	FilterColor1=0,
	// Second Render target for post process filter colors.
	FilterColor2,
	// Third Render target for post process filter colors.
	FilterColor3,
	// Render target for scene colors.
	SceneColor,
	// Render target for scene colors (resolved as raw-bits).
	SceneColorRaw,
	// Render target for scene colors (converted to fixed point).
	SceneColorFixedPoint,
	// Render target for scene depths.
	SceneDepthZ,
	// Render target for a quarter-sized version of the scene depths.
	SmallDepthZ,
	// Depth stencil target for image reflection planar shadowing
	ReflectionSmallDepthZ,
	// Render target for per-object shadow depths.
	ShadowDepthZ,
	// Render target for whole scene dominant shadow depths.
	DominantShadowDepthZ,
	// Render target for dynamic shadow depths on translucency.
	TranslucencyShadowDepthZ,
	// Render target for preshadow depths which are cached across multiple frames.
	PreshadowCacheDepthZ,
	// Render target for one pass point light shadows, 0:at the highest resolution 4:at the lowest resolution
	CubeShadowDepthZ0,
	CubeShadowDepthZ1,
	CubeShadowDepthZ2,
	CubeShadowDepthZ3,
	CubeShadowDepthZ4,
	// Render target for per-object shadow depths as color.
	ShadowDepthColor,
	// Render target for whole scene dominant shadow depths as color.
	DominantShadowDepthColor,
	// Render target for dynamic shadow depths as color on translucency.
	TranslucencyShadowDepthColor,
	// Render target for preshadow depths as color which are cached across multiple frames.
	PreshadowCacheDepthColor,
	// Render target for light attenuation values, also used to store offset vectors for the distortion effect.
	LightAttenuation0,
	// Second render target for light attenuation values, only used with one pass dominant lights
	LightAttenuation1,
	// Render target for downsampled translucency buffer, now also used for half res scene before (postprocessing DOF, Bloom, MotionBlur)
	TranslucencyBuffer,
	// Render target for half res scene before postprocessing (DOF, Bloom, MotionBlur)
	HalfResPostProcess,
	// Render target that stores the dominant light shadow factors for translucency that wants to inherit dynamic shadows from opaque pixels
	TranslucencyDominantLightAttenuation,
	// Render target for ambient occlusion calculations.
	AOInput,
	// Render target for ambient occlusion calculations.
	AOOutput,
	// Render target for ambient occlusion history.
	AOHistory,
	// Render target for motion velocity 2D-vectors.
	VelocityBuffer,
	// Render target for scene colors (resolved at quarter size).
	QuarterSizeSceneColor,
	// Render target that accumulates fog volume frontface integrals
	FogFrontfacesIntegralAccumulation,
	// Render target that accumulates fog volume backface integrals
	FogBackfacesIntegralAccumulation,
	// Render target that hit proxy IDs are drawn to
	HitProxy,
	// Render target for downsampled fog factors
	FogBuffer,
	// Render target for rendering DoF blur factor for translucency (overlaps stencil bits on console)
	DoFBlurBuffer,
	// Procedural texture with stereo offset parameters
	StereoFix,
	// LUTBlend
	LUTBlend,
	// Texture pool memory visualizer
	TexturePoolMemory,
	// The inscattered radiant energy for surfaces with subsurface scattering.
	SubsurfaceInscattering,
	// The scattering attenuation parameters for surfaces with subsurface scattering.
	SubsurfaceScatteringAttenuation,
	// World space normal G Buffer
	WorldNormalGBuffer,
	// World space reflection normal G Buffer
	WorldReflectionNormalGBuffer,
	// Specular color and power G Buffer
	SpecularGBuffer,
	// Diffuse color G Buffer
	DiffuseGBuffer,
	// A 1x1 MSAAed white dummy render target.
	WhiteDummy,
	// Render target for BokehDOF.
	BokehDOF,
	// Render target for SeparateTranslucency (RGB: color of translucent objects, A:blend factor to blend with opaque layer)
	SeparateTranslucency,
	// MSAA-resolved depth buffer (same size as SceneColor, downsampled from the SceneDepth MSAA buffer)
	ResolvedDepthBuffer,
	// The previous frame's scene color, used by temporal AA
	PreviousFrameSceneColor,
	// A copy of the current frame's scene color, used by temporal AA
	CurrentFrameSceneColor,
	// Temporary texture used by scaleform to save off intermediate results when applying layers of filters
	ScaleformTemp,
	// only needed for MLAA (post process anti aliasing) Edge Mask
	MLAAEdgeMask,
	// only needed for MLAA (post process anti aliasing) Edge Count
	MLAAEdgeCount,
	// The capture frame's scene color this is used when the game is paused but wants the scene in the background
	CapturedSceneColor,
	// The depth buffer to use when rendering to offscreen render targets (only necessary on devices that don't allow re-use of the main depth buffer)
	OffscreenDepthBuffer,

	// --- insert new items before this line ---

	// Max scene RTs available
	MAX_SCENE_RENDERTARGETS
};


/** Number of cube map shadow depth surfaces that will be created and used for rendering one pass point light shadows. */
static const INT NumCubeShadowDepthSurfaces = 5;

/**
 * Returns a NULL target if we're not running in stereo. If running in stereo
 * return the specified altBuffer. altBuffer may not be the correct size, and
 * so could trigger warnings or errors in the d3d runtime. However, by providing
 * a buffer that will be unused, we're cueiing the stereo driver that this call
 * should be stereoized.
 */
extern FSurfaceRHIRef StereoizedDrawNullTarget(const FSurfaceRHIRef& altBuffer);

/**
 * In order to address scene render targets outside of FSceneRenderTargets.
 * (should be unified with ESceneRenderTargetTypes, GetFormat/Width/Height/Surface/Texture)
 */
enum FSceneRenderTargetIndex
{
	SRTI_None,
	SRTI_FilterColor0,
	SRTI_FilterColor1,
	SRTI_FilterColor2
};

/**
 * Encapsulates the render targets used for scene rendering.
 */
class FSceneRenderTargets : public FRenderResource
{
public:

	/** Destructor. */
	virtual ~FSceneRenderTargets() {}

	/** 
	 * Dumps information about render target memory usage
	 * Must be called on the rendering thread or while the rendering thread is blocked
	 * Currently only implemented for xbox
	 * @return the amount of memory all of the RenderTargets are utilizing
	 */
	INT DumpMemoryUsage(FOutputDevice& Ar) const;

	/**
	 * Checks that scene render targets are ready for rendering a view family of the given dimensions.
	 * If the allocated render targets are too small, they are reallocated.
	 */
	void Allocate(UINT MinSizeX,UINT MinSizeY);

	/**
	 * Prepares texture pool memory to fit temporal AA allocation, if needed.
	 * Must be called on the gamethread.
	 */
	void PrepareTemporalAAAllocation();

	/**
	 * Updates the allocation for the temporal AA render targets based on whether they are currently needed.
	 *
	 * @param bNeedsAllocation	Whether we want temporal AA to be on this frame.
	 */
	void UpdateTemporalAAAllocation(UBOOL bNeedsAllocation);

	void BeginFrame()
	{
		bResolvedTranslucencyDominantLightAttenuationTexture = FALSE;
	}

	/**
	 * Update the rendertarget based on the specified usage flags.
	 * @param RenderTargetUsage		Bit-wise combination of FSceneRenderTargetUsage flags.
	 */
	void UpdateRenderTargetUsage( FSurfaceRHIParamRef SurfaceRHI, DWORD RenderTargetUsage );

	/**
	 * Sets the current backbuffer for this frame. This needs to be called in the beginning of every frame
	 * when backbuffers are swapped.
	 *
	 * @param InBackBuffer	- Backbuffer to use for the current viewport.
	 * @param InDepthBuffer	- Associated depth buffer for the current viewport.
	 **/
	void SetBackBuffer( FSurfaceRHIParamRef InBackBuffer, FSurfaceRHIParamRef InDepthBuffer );

    /**
     * Sets the backbuffer as the current rendertarget.
	 * @param RenderTargetUsage	- Bit-wise combination of FSceneRenderTargetUsage flags.
     */
    void BeginRenderingBackBuffer( DWORD RenderTargetUsage = RTUsage_Default );
	void SwapCurrentFrameAndPreviousFrameSavedBackBuffers(UINT CurrentFrameNumber);

	void BeginRenderingFilter(FSceneRenderTargetIndex FilterColorIndex);
	void FinishRenderingFilter(FSceneRenderTargetIndex FilterColorIndex);

	void BeginRenderingLUTBlend();
	void FinishRenderingLUTBlend();

	/** Clears the GBuffer render targets to default values. */
	void ClearGBufferTargets();

	/**
	 * Sets the scene color target and restores its contents if necessary
	 * @param RenderTargetUsage	- Bit-wise combination of FSceneRenderTargetUsage flags.
	 * @param bGBufferPass - Whether the pass about to be rendered is the GBuffer population pass
	 * @param bLightingPass - Whether the pass about to be rendered is an additive lighting pass
	 */
	void BeginRenderingSceneColor( DWORD RenderTargetUsage = RTUsage_Default, UBOOL bGBufferPass = FALSE, UBOOL bLightingPass = FALSE );
	/**
	 * Called when finished rendering to the scene color surface
	 * @param bKeepChanges - if TRUE then the SceneColorSurface is resolved to the SceneColorTexture
	 */
	void FinishRenderingSceneColor(UBOOL bKeepChanges = TRUE, const FResolveRect& ResolveRect = FResolveRect());

    /**
     * Resolve a previously rendered scene color surface.
     */
    void ResolveSceneColor(const FResolveRect& ResolveRect = FResolveRect(), UBOOL bKeepOriginalSurface = TRUE);

    /**
     * Resolve the previously rendered subsurface scattering surfaces.
     */
    void ResolveSubsurfaceScatteringSurfaces(const FResolveRect& ResolveRect = FResolveRect());

	/** Resolves the GBuffer targets so that their resolved textures can be sampled. */
	void ResolveGBufferSurfaces(const FResolveRect& ResolveRect = FResolveRect());

    /**
     * Sets the LDR version of the scene color target.
	 * @param RenderTargetUsage	- Bit-wise combination of FSceneRenderTargetUsage flags.
     */
    void BeginRenderingSceneColorLDR( DWORD RenderTargetUsage = RTUsage_Default );
    /**
     * Called when finished rendering to the LDR version of the scene color surface.
     * @param bKeepChanges - if TRUE then the SceneColorSurface is resolved to the LDR SceneColorTexture
	 * @param ResolveParams - optional resolve params
     */
    void FinishRenderingSceneColorLDR(UBOOL bKeepChanges = TRUE, const FResolveRect& ResolveRect = FResolveRect());

	/**
	 * Sets the raw version of the scene color target.
	 */
	void BeginRenderingSceneColorRaw();

	/**
	 * Saves a previously rendered scene color surface in the raw bit format.
	 */
	void SaveSceneColorRaw(UBOOL bConvertToFixedPoint = FALSE, const FResolveRect& ResolveRect = FResolveRect());

	/**
	 * Restores a previously saved raw-scene color surface.
	 */
	void RestoreSceneColorRaw();

	/**
	 * Restores a portion of a previously saved raw-scene color surface.
	 */
	void RestoreSceneColorRectRaw(FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2);

    /** Called to start rendering the depth pre-pass. */
	void BeginRenderingPrePass();

	/** Called when finished rendering the depth pre-pass. */
	void FinishRenderingPrePass();

	void BeginRenderingShadowDepth(UBOOL bIsWholeSceneDominantShadow);

	/** Binds the appropriate shadow depth render target for rendering to the preshadow cache, and sets up color write enable state. */
	void BeginRenderingPreshadowCacheDepth();

	/** Binds the appropriate shadow depth cube map for rendering. */
	void BeginRenderingCubeShadowDepth(INT ShadowResolution);

	/**
	 * Called when finished rendering to the subject shadow depths so the surface can be copied to texture
	 * @param ResolveParams - optional resolve params
	 */
	void FinishRenderingShadowDepth(UBOOL bIsWholeSceneDominantShadow, const FResolveRect& ResolveRect = FResolveRect());

	/**
	 * Resolve the preshadow cache depth surface
	 * @param ResolveParams - optional resolve params
	 */
	void ResolvePreshadowCacheDepth(const FResolveParams& ResolveParams = FResolveParams());

	/** Resolves the approprate shadow depth cube map and restores default state. */
	void FinishRenderingCubeShadowDepth(INT ShadowResolution, const FResolveParams& ResolveParams = FResolveParams());

	void BeginRenderingLightAttenuation(UBOOL bUseTexture0 = TRUE);
	void FinishRenderingLightAttenuation(UBOOL bUseTexture0 = TRUE);

	void BeginRenderingTranslucency(const FViewInfo& View, UBOOL bDownSampled, UBOOL bStateChanged);
	void FinishRenderingTranslucency(const FResolveParams& ResolveParams, UBOOL bDownSampled);

	void BeginRenderingSeparateTranslucency(const FViewInfo& View);
	void FinishRenderingSeparateTranslucency();
	void ResolveFullResTransluceny();

	void BeginRenderingAOInput(UBOOL bUseDownsizedDepthBuffer = FALSE);
	void FinishRenderingAOInput(const FResolveRect& ResolveRect);

	void BeginRenderingAOOutput(UBOOL bUseDownsizedDepthBuffer = FALSE);
	void FinishRenderingAOOutput(const FResolveRect& ResolveRect);

	void BeginRenderingAOHistory(UBOOL bUseDownsizedDepthBuffer = FALSE);
	void FinishRenderingAOHistory(const FResolveParams& ResolveParams);

	void BeginRenderingDistortionAccumulation();
	void FinishRenderingDistortionAccumulation(const FResolveRect& ResolveRect = FResolveRect());

	void BeginRenderingDistortionDepth();
	void FinishRenderingDistortionDepth();

	/** Starts rendering to the velocity buffer. */
	void BeginRenderingVelocities();

	/** Stops rendering to the velocity buffer. */
	void FinishRenderingVelocities();

	void BeginRenderingFogFrontfacesIntegralAccumulation();
	void FinishRenderingFogFrontfacesIntegralAccumulation();

	void BeginRenderingFogBackfacesIntegralAccumulation();
	void FinishRenderingFogBackfacesIntegralAccumulation();

	void ResolveSceneDepthTexture();

	void BeginRenderingHitProxies();
	void FinishRenderingHitProxies();

	void BeginRenderingFogBuffer();
	void FinishRenderingFogBuffer(const FResolveParams& ResolveParams);

	void BeginRenderingPostTranslucencyDepth();
	void FinishRenderingPostTranslucencyDepth(UBOOL bKeep,const FResolveParams& ResolveParams = FResolveParams());

	void BeginRenderingDoFBlurBuffer();
	void FinishRenderingDoFBlurBuffer();

	void OverrideSceneColorSurface(const FSurfaceRHIRef& OverrideSceneColor);
	void ClearSceneColorSurfaceOverride();

	void OverrideSceneDepthSurface(const FSurfaceRHIRef& OverrideSceneDepth);
	void ClearSceneDepthSurfaceOverride();

	/**
	 * Creates/releases render targets based on the current System Settings
	 */
	void ApplySystemSettings();

	// FRenderResource interface.
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

	// Texture Accessors -----------

	const FTexture2DRHIRef& GetFilterColorTexture(FSceneRenderTargetIndex Index) const;
	const FTexture2DRHIRef& GetSceneColorRawTexture() const { return RenderTargets[SceneColorRaw].Texture; }

	/** Returns the current effective scene color texture, taking into account which texture scene color was last resolved into. */
	const FTexture2DRHIRef& GetEffectiveSceneColorTexture() const 
	{ 
#if XBOX
		if (bSceneColorTextureIsRaw)
		{
			return RenderTargets[SceneColorRaw].Texture; 
		}
		else
#endif
		{
			return RenderTargets[SceneColor].Texture; 
		}
	}

	const FTexture2DRHIRef& GetScaleformTempTexture() const { return RenderTargets[ScaleformTemp].Texture; }
	const FTexture2DRHIRef& GetSceneColorTexture() const { return RenderTargets[SceneColor].Texture; }
	const FTexture2DRHIRef& GetSceneColorLDRTexture() const { return RenderTargets[LightAttenuation0].Texture; }
	const FTextureRHIRef& GetLUTBlendTexture() const;
	const FTexture2DRHIRef& GetSceneDepthTexture() const { return RenderTargets[SceneDepthZ].Texture; }
    const FTexture2DRHIRef& GetStereoFixTexture() const { return RenderTargets[StereoFix].Texture; } 
	const FTexture2DRHIRef& GetShadowDepthZTexture(UBOOL bIsWholeSceneDominantShadow, UBOOL bCachedPreshadow) const 
	{ 
		if (bCachedPreshadow)
		{
			return RenderTargets[PreshadowCacheDepthZ].Texture;
		}
		else
		{
			return bIsWholeSceneDominantShadow ? RenderTargets[DominantShadowDepthZ].Texture : RenderTargets[ShadowDepthZ].Texture; 
		}
	}
	const FTexture2DRHIRef& GetTranslucencyShadowDepthZTexture(UBOOL bCachedPreshadow) const 
	{ 
		return bCachedPreshadow ? RenderTargets[PreshadowCacheDepthZ].Texture : RenderTargets[TranslucencyShadowDepthZ].Texture; 
	}
	const FTextureCubeRHIRef& GetCubeShadowDepthZTexture(INT ShadowResolution) const 
	{ 
		return RenderTargets[CubeShadowDepthZ0 + GetCubeShadowDepthZIndex(ShadowResolution)].TextureCube; 
	}
	const FTexture2DRHIRef& GetShadowDepthColorTexture(UBOOL bIsWholeSceneDominantShadow, UBOOL bCachedPreshadow) const 
	{ 
		if (bCachedPreshadow)
		{
			return RenderTargets[PreshadowCacheDepthColor].Texture;
		}
		else
		{
			return bIsWholeSceneDominantShadow ? RenderTargets[DominantShadowDepthColor].Texture : RenderTargets[ShadowDepthColor].Texture; 
		}
	}
	const FTexture2DRHIRef& GetTranslucencyShadowDepthColorTexture(UBOOL bCachedPreshadow) const 
	{ 
		return bCachedPreshadow ? RenderTargets[PreshadowCacheDepthColor].Texture : RenderTargets[TranslucencyShadowDepthColor].Texture; 
	}
	const FTexture2DRHIRef& GetLightAttenuationTexture() const { return RenderTargets[LightAttenuation0].Texture; }
	const FTexture2DRHIRef& GetTranslucencyBufferTexture() const { return RenderTargets[TranslucencyBuffer].Texture; }
	const FTexture2DRHIRef& GetHalfResPostProcessTexture() const { return RenderTargets[HalfResPostProcess].Texture; }
	const FTexture2DRHIRef& GetAOInputTexture() const { return RenderTargets[AOInput].Texture; }
	const FTexture2DRHIRef& GetAOOutputTexture() const { return RenderTargets[AOOutput].Texture; }
	const FTexture2DRHIRef& GetAOHistoryTexture() const { return RenderTargets[AOHistory].Texture; }
	const FTexture2DRHIRef& GetVelocityTexture() const { return RenderTargets[VelocityBuffer].Texture; }
	const FTexture2DRHIRef& GetQuarterSizeSceneColorTexture() const { return RenderTargets[QuarterSizeSceneColor].Texture; }
	const FTexture2DRHIRef& GetFogFrontfacesIntegralAccumulationTexture() const { return RenderTargets[FogFrontfacesIntegralAccumulation].Texture; }	
	const FTexture2DRHIRef& GetFogBackfacesIntegralAccumulationTexture() const { return RenderTargets[FogBackfacesIntegralAccumulation].Texture; }
	const FTexture2DRHIRef& GetHitProxyTexture() const { return RenderTargets[HitProxy].Texture; }
	const FTexture2DRHIRef& GetFogBufferTexture() const { return RenderTargets[FogBuffer].Texture; }
	const FTexture2DRHIRef& GetDoFBlurBufferTexture() const	{ return RenderTargets[DoFBlurBuffer].Texture; }
	const FTexture2DRHIRef& GetTranslucencyDominantLightAttenuationTexture() const	{ return RenderTargets[TranslucencyDominantLightAttenuation].Texture; }

	const FTexture2DRHIRef& GetSubsurfaceInscatteringTexture() const { return RenderTargets[SubsurfaceInscattering].Texture; }
	const FTexture2DRHIRef& GetSubsurfaceScatteringAttenuationTexture() const { return RenderTargets[SubsurfaceScatteringAttenuation].Texture; }
	const FTexture2DRHIRef& GetWorldNormalGBufferTexture() const { return RenderTargets[WorldNormalGBuffer].Texture; }
	const FTexture2DRHIRef& GetWorldReflectionNormalGBufferTexture() const { return RenderTargets[WorldReflectionNormalGBuffer].Texture; }
	const FTexture2DRHIRef& GetSpecularGBufferTexture() const { return RenderTargets[SpecularGBuffer].Texture; }
	const FTexture2DRHIRef& GetDiffuseGBufferTexture() const { return RenderTargets[DiffuseGBuffer].Texture; }
	const FTexture2DRHIRef& GetBokehDOFTexture() const { return RenderTargets[BokehDOF].Texture; }
	const FTexture2DRHIRef& GetSeparateTranslucencyTexture() const { return RenderTargets[SeparateTranslucency].Texture; }
	const FTexture2DRHIRef& GetResolvedDepthTexture() const { return RenderTargets[ResolvedDepthBuffer].Texture; }

	/** Updates the FSceneRenderTargets::TexturePoolMemory texture, if it's currently being visualized on-screen. */
	void UpdateTexturePoolVisualizer();

	/* Useful to clear the content, as the right Clear neeeds to be called depening on this. */
	UBOOL IsRenderTargetADepthTexture(ESceneRenderTargetTypes Index) const;

	/** @return IsValidRef()=FALSE if out of bounds, for debugging */
	const FTexture2DRHIRef& GetRenderTargetTexture(ESceneRenderTargetTypes Index) const;
	/** @return IsValidRef()=FALSE if out of bounds, for debugging */
	const FSurfaceRHIRef& GetRenderTargetSurface(ESceneRenderTargetTypes Index) const;
	/** Returns a string matching name of the rendertarget Index and it's extent (SizeX, SizeY), (0,0) means unknown, for debugging */
	FString GetRenderTargetInfo(ESceneRenderTargetTypes Index, FIntPoint &OutExtent) const;

	/** 
	* Allows substitution of a 1x1 white texture in place of the light attenuation buffer when it is not needed;
	* this improves shader performance and removes the need for redundant Clears
	*/
	void SetLightAttenuationMode(UBOOL bEnabled) { bLightAttenuationEnabled = bEnabled; }
	const FTextureRHIRef& GetEffectiveLightAttenuationTexture(UBOOL bReceiveDynamicShadows, UBOOL bUseLightAttenuation0) const 
	{
		if( bLightAttenuationEnabled && bReceiveDynamicShadows )
		{
			return *(FTextureRHIRef*)&RenderTargets[bUseLightAttenuation0 ? LightAttenuation0 : LightAttenuation1].Texture;
		}
		else
		{
			return GWhiteTexture->TextureRHI;
		}
	}
	const FSurfaceRHIRef& GetEffectiveLightAttenuationSurface(UBOOL bReceiveDynamicShadows, UBOOL bUseLightAttenuation0) const 
	{
		if( bLightAttenuationEnabled && bReceiveDynamicShadows )
		{
			return RenderTargets[bUseLightAttenuation0 ? LightAttenuation0 : LightAttenuation1].Surface;
		}
		else
		{
			return RenderTargets[WhiteDummy].Surface;
		}
	}

	UBOOL IsWholeSceneDominantShadowValid() const { return bWholeSceneDominantShadowValid; }
	void SetWholeSceneDominantShadowValid(UBOOL bInWholeSceneDominantShadowValid) 
	{ 
		bWholeSceneDominantShadowValid = bInWholeSceneDominantShadowValid;
	}

	UBOOL IsDownsizedDepthSupported() const { return bDownsizedDepthSupported; }
	UBOOL UseDownsizedOcclusionQueries() const { return bUseDownsizedOcclusionQueries; }

	// Surface Accessors ---------------

	const FSurfaceRHIRef& GetBackBuffer() const									{ return BackBuffer; }

	const FSurfaceRHIRef& GetFilterColorSurface(FSceneRenderTargetIndex FilterColorIndex) const;
	const FSurfaceRHIRef& GetSceneColorRawSurface() const						{ return RenderTargets[SceneColorRaw].Surface; }
	const FSurfaceRHIRef& GetSceneColorSurface() const							{ return RenderTargets[SceneColor].Surface; }
	const FSurfaceRHIRef& GetSceneColorLDRSurface() const						{ return RenderTargets[LightAttenuation0].Surface; }
	const FSurfaceRHIRef& GetSceneDepthSurface() const							{ return RenderTargets[SceneDepthZ].Surface; }
	const FSurfaceRHIRef& GetSmallDepthSurface() const							{ return RenderTargets[SmallDepthZ].Surface; }
	const FSurfaceRHIRef& GetReflectionSmallDepthSurface() const				{ return RenderTargets[ReflectionSmallDepthZ].Surface; }
	const FSurfaceRHIRef& GetShadowDepthZSurface(UBOOL bIsWholeSceneDominantShadow) const						
	{ 
		return bIsWholeSceneDominantShadow ? RenderTargets[DominantShadowDepthZ].Surface : RenderTargets[ShadowDepthZ].Surface; 
	}
	const FSurfaceRHIRef& GetTranslucencyShadowDepthZSurface() const						
	{ 
		return RenderTargets[TranslucencyShadowDepthZ].Surface; 
	}
	const FSurfaceRHIRef& GetPreshadowCacheDepthZSurface() const						
	{ 
		return RenderTargets[PreshadowCacheDepthZ].Surface; 
	}
	const FSurfaceRHIRef& GetShadowDepthColorSurface(UBOOL bIsWholeSceneDominantShadow) const					
	{ 
		return bIsWholeSceneDominantShadow ? RenderTargets[DominantShadowDepthColor].Surface : RenderTargets[ShadowDepthColor].Surface; 
	}
	const FSurfaceRHIRef& GetTranslucencyShadowDepthColorSurface() const					
	{ 
		return RenderTargets[TranslucencyShadowDepthColor].Surface; 
	}
	const FSurfaceRHIRef& GetPreshadowCacheDepthColorSurface() const					
	{ 
		return RenderTargets[PreshadowCacheDepthColor].Surface; 
	}
	const FSurfaceRHIRef& GetCubeShadowDepthZSurface(INT ShadowResolution) const						
	{ 
		return RenderTargets[CubeShadowDepthZ0 + GetCubeShadowDepthZIndex(ShadowResolution)].Surface; 
	}
	const FSurfaceRHIRef& GetLUTBlendSurface() const							{ return RenderTargets[LUTBlend].Surface; }
	const FSurfaceRHIRef& GetLightAttenuationSurface(UBOOL bUseTexture0 = TRUE) const { return RenderTargets[bUseTexture0 ? LightAttenuation0 : LightAttenuation1].Surface; }
	const FSurfaceRHIRef& GetTranslucencyBufferSurface() const					{ return RenderTargets[TranslucencyBuffer].Surface; }
	const FSurfaceRHIRef& GetHalfResPostProcessSurface() const					{ return RenderTargets[HalfResPostProcess].Surface; }
	const FSurfaceRHIRef& GetAOInputSurface() const								{ return RenderTargets[AOInput].Surface; }
	const FSurfaceRHIRef& GetAOOutputSurface() const							{ return RenderTargets[AOOutput].Surface; }
	const FSurfaceRHIRef& GetAOHistorySurface() const							{ return RenderTargets[AOHistory].Surface; }
	const FSurfaceRHIRef& GetVelocitySurface() const							{ return RenderTargets[VelocityBuffer].Surface; }
	const FSurfaceRHIRef& GetQuarterSizeSceneColorSurface() const				{ return RenderTargets[QuarterSizeSceneColor].Surface; }
	const FSurfaceRHIRef& GetFogFrontfacesIntegralAccumulationSurface() const	{ return RenderTargets[FogFrontfacesIntegralAccumulation].Surface; }
	const FSurfaceRHIRef& GetFogBackfacesIntegralAccumulationSurface() const	{ return RenderTargets[FogBackfacesIntegralAccumulation].Surface; }
	const FSurfaceRHIRef& GetHitProxySurface() const							{ return RenderTargets[HitProxy].Surface; }
	const FSurfaceRHIRef& GetFogBufferSurface() const							{ return RenderTargets[FogBuffer].Surface; }
	const FSurfaceRHIRef& GetDoFBlurBufferSurface() const						{ return RenderTargets[DoFBlurBuffer].Surface; }

	const FSurfaceRHIRef& GetSubsurfaceInscatteringSurface() const				{ return RenderTargets[SubsurfaceInscattering].Surface; }
	const FSurfaceRHIRef& GetSubsurfaceScatteringAttenuationSurface() const		{ return RenderTargets[SubsurfaceScatteringAttenuation].Surface; }
	const FSurfaceRHIRef& GetWorldNormalGBufferSurface() const					{ return RenderTargets[WorldNormalGBuffer].Surface; }
	const FSurfaceRHIRef& GetWorldReflectionNormalGBufferSurface() const			{ return RenderTargets[WorldReflectionNormalGBuffer].Surface; }
	const FSurfaceRHIRef& GetSpecularGBufferSurface() const						{ return RenderTargets[SpecularGBuffer].Surface; }
	const FSurfaceRHIRef& GetDiffuseGBufferSurface() const						{ return RenderTargets[DiffuseGBuffer].Surface; }
	const FSurfaceRHIRef& GetBokehDOFSurface() const							{ return RenderTargets[BokehDOF].Surface; }
	const FSurfaceRHIRef& GetSeparateTranslucencySurface() const				{ return RenderTargets[SeparateTranslucency].Surface; }
	const FSurfaceRHIRef& GetResolvedDepthSurface() const						{ return RenderTargets[ResolvedDepthBuffer].Surface; }

	/** Returns the size of the shadow depth buffer, taking into account platform limitations and game specific resolution limits. */
	FIntPoint GetShadowDepthTextureResolution(UBOOL bWholeSceneDominantShadow = FALSE) const;
	/** Returns the dimensions of the translucency shadow depth texture and surface. */
	FIntPoint GetTranslucencyShadowDepthTextureResolution() const;
	/** Returns the dimensions of the preshadow cache texture and surface. */
	FIntPoint GetPreshadowCacheTextureResolution() const;

	UBOOL IsHardwarePCFSupported() const;
	inline UBOOL IsFetch4Supported() const 
	{
		return GSystemSettings.bAllowHardwareShadowFiltering && GSupportsFetch4;
	}

	UINT GetBufferSizeX() const { return BufferSizeX; }
	UINT GetBufferSizeY() const { return BufferSizeY; }
	void SetBufferSize( const UINT InBufferSizeX, const UINT InBufferSizeY );
	void SetSceneColorBufferFormat(EPixelFormat InFormat)
	{
		check(!IsInitialized() || !GIsRHIInitialized);
		SceneColorBufferFormat = InFormat;
	}

	UINT GetSmallColorDepthDownsampleFactor() const { return SmallColorDepthDownsampleFactor; }

	UINT GetFilterDownsampleFactor() const { return FilterDownsampleFactor; }

	UINT GetFilterBufferSizeX() const { return FilterBufferSizeX; }
	UINT GetFilterBufferSizeY() const { return FilterBufferSizeY; }

	UINT GetVelocityBufferSizeX() const { return VelocityBufferSizeX; }
	UINT GetVelocityBufferSizeY() const { return VelocityBufferSizeY; }

	UINT GetFogAccumulationDownsampleFactor() const { return FogAccumulationDownsampleFactor; }
	UINT GetFogAccumulationBufferSizeX() const { return FogAccumulationBufferSizeX; }
	UINT GetFogAccumulationBufferSizeY() const { return FogAccumulationBufferSizeY; }

	UINT GetTranslucencyBufferSizeX() const { return TranslucencyBufferSizeX; }
	UINT GetTranslucencyBufferSizeY() const { return TranslucencyBufferSizeY; }

	UINT GetAODownsampleFactor() const { return AODownsampleFactor; }
	void SetAODownsampleFactor(INT NewDownsampleFactor);
	UINT GetAOBufferSizeX() const { return AOBufferSizeX; }
	UINT GetAOBufferSizeY() const { return AOBufferSizeY; }

	/** Returns an index in the range [0, NumCubeShadowDepthSurfaces) given an input resolution. */
	INT GetCubeShadowDepthZIndex(INT ShadowResolution) const;

	/** Returns the appropriate resolution for a given cube shadow index. */
	INT GetCubeShadowDepthZResolution(INT ShadowIndex) const;

	UBOOL IsSeparateTranslucencyActive() const;

	UBOOL IsPreviousFrameSceneColorValid(UINT CurrentFrameNumber) const { return (PreviousFrameSceneColorFrameIndex + 1) == CurrentFrameNumber; }

	/** TRUE if the ambient occlusion history render target needs to be cleared before next use. */
	UBOOL bAOHistoryNeedsCleared;

	/** TRUE if the shared surface memory used for SceneColor and SceneColorRaw contains the raw format. */
	UBOOL bSceneColorTextureIsRaw;

	/** TRUE if TranslucencyDominantLightAttenuationTexture has been populated with meaningful results. */
	UBOOL bResolvedTranslucencyDominantLightAttenuationTexture;

	FPreviousPerBoneMotionBlur PrevPerBoneMotionBlur;

protected:
	// Constructor.
	FSceneRenderTargets(): 
		bAOHistoryNeedsCleared(FALSE),
		bSceneColorTextureIsRaw(FALSE),
		bResolvedTranslucencyDominantLightAttenuationTexture(FALSE),
		BufferSizeX(0), 
		BufferSizeY(0), 
		FilterDownsampleFactor(0), 
		FilterBufferSizeX(0), 
		FilterBufferSizeY(0), 
		VelocityBufferSizeX(0),
		VelocityBufferSizeY(0),
		SmallColorDepthDownsampleFactor(2),
		AODownsampleFactor(2),
		LightAttenuationMemoryBuffer(NULL),
		SceneColorBufferFormat(PF_FloatRGB),
		bLightAttenuationEnabled(TRUE),
		bWholeSceneDominantShadowValid(FALSE),
		bDownsizedDepthSupported(FALSE),
		bUseDownsizedOcclusionQueries(FALSE),
		PreviousFrameSceneColorFrameIndex(MININT),
		bIsTemporalAAAllocated(0)
		{}

private:

	// size of the back buffer, in editor this has to be >= than the biggest view port
	UINT BufferSizeX;
	UINT BufferSizeY;

	// e.g. 4
	UINT FilterDownsampleFactor;
	// back buffer size down sampled by "FilterDownsampleFactor" with additional border
	UINT FilterBufferSizeX;
	UINT FilterBufferSizeY;

	// back buffer size down sampled by some factor (e.g. 2 on Xbox360, 1 by default)
	UINT VelocityBufferSizeX;
	UINT VelocityBufferSizeY;

	// e.g. 2
	UINT FogAccumulationDownsampleFactor;
	// back buffer size down sampled by "FogAccumulationDownsampleFactor"
	UINT FogAccumulationBufferSizeX;
	UINT FogAccumulationBufferSizeY;

	// e.g. 2
	UINT SmallColorDepthDownsampleFactor;
	// back buffer size down sampled by "SmallColorDepthDownsampleFactor"
	UINT TranslucencyBufferSizeX;
	UINT TranslucencyBufferSizeY;

	// e.g. 2
	UINT AODownsampleFactor;
	// back buffer size down sampled by "AODownsampleFactor"
	UINT AOBufferSizeX;
	UINT AOBufferSizeY;

	FSharedMemoryResourceRHIRef LightAttenuationMemoryBuffer;
	FSharedMemoryResourceRHIRef LUTMemoryBuffer;
	FSharedMemoryResourceRHIRef PreviousFrameSceneColorMemoryBuffer;

	EPixelFormat SceneColorBufferFormat;

	enum ESceneRenderTargetFlags
	{

	};

	/**
	* Single render target item consists of a render surface and its resolve texture
	*/
	struct FSceneRenderTargetItem
	{
		FSceneRenderTargetItem() 
			: Flags(0) 
		{
		}
		/** texture for resolving to */
		FTexture2DRHIRef Texture;
#if XBOX && !USE_NULL_RHI
		/** texture array for resolving to */
		FTexture2DArrayRHIRef TextureArray;
#endif // XBOX && !USE_NULL_RHI

		/** Cube texture RHI reference, if this is a cube map surface. */
		FTextureCubeRHIRef TextureCube;

		/** surface to render to */
		FSurfaceRHIRef Surface;
        /** combination of ESceneRenderTargetFlags */
		DWORD Flags;
	};
	/** static array of all the scene render targets */
	FSceneRenderTargetItem RenderTargets[MAX_SCENE_RENDERTARGETS];
	/** Current backbuffer */
	FSurfaceRHIRef BackBuffer;
	/** if TRUE we use the light attenuation buffer otherwise the 1x1 white texture is used */
	UBOOL bLightAttenuationEnabled;
	/** Indicates whether the whole scene dominant shadow projection in the light attenuation alpha is valid. */
	UBOOL bWholeSceneDominantShadowValid;
	/** Whether the render target corresponding to SmallDepthZ is supported. */
	UBOOL bDownsizedDepthSupported;
	/** Whether to use SmallDepthZ for occlusion queries. */
	UBOOL bUseDownsizedOcclusionQueries;

	/** The index of the frame that is stored in PreviousFrameSceneColor. */
	INT PreviousFrameSceneColorFrameIndex;

	/** Thread-safe flag for whether the shared memory for Temporal AA is allocated or not. */
	volatile INT bIsTemporalAAAllocated;

	/** Allocates ambient occlusion buffers if allowed */
	void AllocateAOBuffers();
	/** Returns a string matching the given ESceneRenderTargetTypes */
	FString GetRenderTargetName(ESceneRenderTargetTypes RTEnum) const;

	/** Creates all scene render targets on mobile devices. */
	void InitDynamicRHIMobile();

	// These are the texture references for the render target textures that are currently aliased by PreviousFrameSceneColor.
	FTexture2DRHIRef LightAttenuation0_OverlappingPreviousFrameSceneColor;
	FTexture2DRHIRef FogFrontfacesIntegralAccumulation_OverlappingPreviousFrameSceneColor;
	FTexture2DRHIRef ShadowDepthZ_OverlappingPreviousFrameSceneColor;
	FTexture2DRHIRef AOInput_OverlappingPreviousFrameSceneColor;
	FTexture2DRHIRef AOOutput_OverlappingPreviousFrameSceneColor;

	// These are surface and texture references to backup the scene color and depth render target items while they
	// are replaced by the capture surfaces.
	FSceneRenderTargetItem BackupSceneColor;
	FSceneRenderTargetItem BackupSceneDepthZ;
};

/** The global render targets used for scene rendering. */
extern TGlobalResource<FSceneRenderTargets> GSceneRenderTargets;

/** Vertex shader parameters needed for deferred passes. */
class FDeferredVertexShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	void Set(const FSceneView& View, FShader* VertexShader) const;

	friend FArchive& operator<<(FArchive& Ar,FDeferredVertexShaderParameters& P);

private:
	FShaderParameter ScreenToWorldParameter;
};

/** Pixel shader parameters needed for deferred passes. */
class FDeferredPixelShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	void Set(const FSceneView& View, FShader* PixelShader, ESceneDepthUsage SceneDepthUsage = SceneDepthUsage_Normal) const;

	friend FArchive& operator<<(FArchive& Ar,FDeferredPixelShaderParameters& P);

//@HACK: Exposed for OpenGL ES 2
	FSceneTextureShaderParameters SceneTextureParameters;
private:
	FShaderResourceParameter LightAttenuationSurface;
	FShaderResourceParameter WorldNormalGBufferTextureMS;
	FShaderResourceParameter WorldReflectionNormalGBufferTextureMS;
	FShaderResourceParameter SpecularGBufferTextureMS;
	FShaderResourceParameter DiffuseGBufferTextureMS;
	FShaderResourceParameter WorldNormalGBufferTexture;
	FShaderResourceParameter WorldReflectionNormalGBufferTexture;
	FShaderResourceParameter SpecularGBufferTexture;
	FShaderResourceParameter DiffuseGBufferTexture;
	FShaderParameter ScreenToWorldParameter;
};

/**
* Proxy render target that wraps an existing render target RHI resource
*/
class FSceneRenderTargetProxy : public FRenderTarget
{
public:
	/**
	* Constructor
	*/
	FSceneRenderTargetProxy();

	/**
	* Set SizeX and SizeY of proxy and re-allocate scene targets as needed
	*
	* @param InSizeX - scene render target width requested
	* @param InSizeY - scene render target height requested
	*/
	void SetSizes(UINT InSizeX,UINT InSizeY);

	// FRenderTarget interface

	/**
	* @return width of the scene render target this proxy will render to
	*/
	virtual UINT GetSizeX() const;

	/**
	* @return height of the scene render target this proxy will render to
	*/
	virtual UINT GetSizeY() const;	

	/**
	* @return gamma this render target should be rendered with
	*/
	virtual FLOAT GetDisplayGamma() const;

	/**
	* @return RHI surface for setting the render target
	*/
	virtual const FSurfaceRHIRef& GetRenderTargetSurface() const;

private:

	/** scene render target width requested */
	UINT SizeX;
	/** scene render target height requested */
	UINT SizeY;
};

/**
* Proxy render target that wraps an existing scene depth buffer surface RHI resource
*/
class FSceneDepthTargetProxy
{
public:
	/**
	* @return RHI surface for setting the render target
	*/
	const FSurfaceRHIRef& GetDepthTargetSurface() const;
};


/**
 * Alternates every few seconds between TRUE/FALSE to allow profiling purely
 * useful on platforms that don't support any other profiling features.
 * see console variable: "MobileProfile"
 * @param LocalFeatureMask e.g. 0x10 see console variable "ProfileEx"
 * @param CurrentRealTime usually from View->Family->CurrentRealTime
 * @return TRUE to normal rendering, FALSE: disable the feature
 */

#if !FINAL_RELEASE

	extern UBOOL TestProfileExState(UINT LocalFeatureMask, FLOAT CurrentRealTime);
	#define TEST_PROFILEEXSTATE(MASK, TIME) TestProfileExState(MASK, TIME)

#else

	#define TEST_PROFILEEXSTATE(MASK, TIME) TRUE

#endif

#endif
