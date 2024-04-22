/*=============================================================================
	ES2RHIPrivate.h: Private OpenGL ES 2.0 RHI declarations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __ES2RHIPRIVATE_H__
#define __ES2RHIPRIVATE_H__

#include "ES2RHI.h"
#include "ES2RHIDebug.h"

#if WITH_ES2_RHI

////////////////////////////////////
//
// OpenGL extensions
// 
////////////////////////////////////

// http://www.khronos.org/registry/gles/extensions/IMG/IMG_texture_compression_pvrtc.txt
#if !defined(GL_IMG_texture_compression_pvrtc)
	#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG		0x8C00
	#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG		0x8C01
	#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG		0x8C02
	#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG		0x8C03
#endif

// http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt
#if !defined(GL_EXT_texture_compression_s3tc)
	#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT			0x83F0
	#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT		0x83F1
	#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT		0x83F2
	#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT		0x83F3
#endif

// http://www.khronos.org/registry/gles/extensions/AMD/AMD_compressed_ATC_texture.txt
#if !defined(GL_ATI_texture_compression_atitc)
	#define GL_ATC_RGB_AMD							0x8C92
	#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD			0x8C93
	#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD		0x87EE
#endif

// Grabbed from gl2ext.h in android headers
#if !defined(GL_OES_compressed_ETC1_RGB8_texture)
	#define GL_ETC1_RGB8_OES                        0x8D64
#endif

#if !defined(GL_OES_texture_half_float)
	#define GL_HALF_FLOAT_OES                       0x8D61
#endif

////////////////////////////////////
//
// Types
// 
////////////////////////////////////

/** Flags that define base features for a set of shader permutations */
namespace EShaderBaseFeatures
{
	/** EShaderBaseFeatures::Type */
	typedef UINT Type;

	/** Default */
	const Type Default = 0;

	/** Lightmap is supported */
	const Type Lightmap = 1 << 0;

	/** Directional Lightmap is supported */
	const Type DirectionalLightmap = 1 << 1;

	/** Vertex lightmap is supported */
	//const Type VertexLightmap = 1 << 1;

	/** GPU skinning is supported */
	const Type GPUSkinning = 1 << 2;

	/** Decals are supported */
	const Type Decal = 1 << 3;

	/** SubUV particles are supported */
	const Type SubUVParticles = 1 << 4;

	/** Landscape is supported */
	const Type Landscape = 1 << 5;
}

struct FStateShadow 
{
	FStateShadow();

	void InvalidateAndResetDevice(void);

	FRasterizerStateInitializerRHI Rasterizer;
	FDepthStateInitializerRHI Depth;
	FBlendStateInitializerRHI Blend;

	UINT			RenderTargetWidth;
	UINT			RenderTargetHeight;

	FSurfaceRHIParamRef	CurrentRenderTargetRHI;
	FSurfaceRHIParamRef	CurrentDepthTargetRHI;
	INT				CurrentRenderTargetID;
	INT				CurrentDepthStencilTargetID;

	UBOOL 			ColorWriteEnable;
	UINT  			ColorWriteMask;

	/**
	 * TRUE if we're using a dummy depth buffer instead a NULL buffer.
	 * While TRUE, depth and stencil usage will be disabled and any user depth/stencil settings will be ignored.
	 */
	UBOOL			bIsUsingDummyDepthStencilBuffer;

	GLuint 			ActiveTexture;
	GLuint 			BoundTextureName[MAX_MobileTexture];
	GLuint 			BoundTextureType[MAX_MobileTexture];
	EPixelFormat	BoundTextureFormat[MAX_MobileTexture];

	/** This field tracks which texture slots have DXT5 textures bound to them */
	DWORD			BoundTextureMask;
#if !FINAL_RELEASE
	INT				LastUpdatePrimitive[MAX_Mapped_MobileTexture];
#endif

	GLuint 			ElementArrayBuffer;
	GLuint 			ArrayBuffer[16];
	GLint			VertexAttribCount[16];
	GLenum			VertexAttribFormat[16];
	GLboolean		VertexAttribNormalize[16];
	GLsizei			VertexAttribStride[16];
	const GLvoid*	VertexAttribAddress[16];
};

extern FStateShadow GStateShadow;


class FES2ShaderManager
{
public:

	/**
	 * Constructor
	 */
	 FES2ShaderManager();
	 
	/** 
	 * Perform global initialization for the shader manager (called from the main InitRHI)
	 */
	void InitRHI();
	 
	/** 
	 * Perform global initialization for all programs
	 */
	void InitShaderPrograms();
	void InitGlobalShaderPrograms();

	/** 
	 * Clears out any GPU Resources used by Shader Manager
	 */
	void ClearGPUResources();

	/**
	 * Discovers and initializes all preprocessed shader program instances
	 */
	void InitPreprocessedShaderPrograms();
	void InitPreprocessedShaderProgram(const FProgramKey & ProgramKey);
	void ClearCompiledShaders();
	void ClearCompiledShader(const FProgramKey & ProgramKey);
	void WarmShaderCache();

	/**
	 * Clears all shader programs, forcing them to be reconstructed on demand from source
	 */
	void ClearShaderProgramInstances();

	/**
	 *  Explicit Call to reset Platform features during a runtime settings change
	 */
	void ResetPlatformFeatures();

	/**
	 * Set the current program
	 *
	 * @param Type The primitive type that will be rendered
	 * @param InGlobalShaderType The type of global shader if this is non-material program
	 * @return True if the program has changed
	 */
	UBOOL SetProgramByType(EMobilePrimitiveType Type, EMobileGlobalShaderType InGlobalShaderType);
	
	/**
	 * Set the alpha test information for upcoming draw calls
	 *
	 * @param bEnable Whether or not to enable alpha test
	 * @param AlphaRef Value to compare against for alpha kill
	 */
	void SetAlphaTest(UBOOL bEnable, FLOAT AlphaRef);

	/**
	 * Resets ALL state and just sets state based on blendmode
	 *
	 * @param InBlendMode - Material BlendMode
	 */
	void SetMobileSimpleParams(const EBlendMode InBlendMode);

	/**
	 * Set blend mode enum for shader key/patching purposes
	 */
	void SetMobileBlendMode(EBlendMode InBlendMode)
	{
		BlendMode = InBlendMode;
	}

	/**
	 * Sets up the vertex shader state for GL system
	 *
	 * @param InVertexParams - A composite structure of the mobile vertex parameters needed by GL shaders
	 */
	void SetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams);

	/**
	 * Sets up the material pixel shader state for GL system
	 *
	 * @param InPixelParams - A composite structure of the mobile pixel parameters needed by GL shaders
	 */
	void SetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams);

	/**
	 * Sets up vertex shader state for mesh parameters
	 *
	 * @param InMeshParams - A composite structure of the mobile mesh parameters needed by GL shaders
	 */
	void SetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams);

	/**
	 * Sets up pixel shader state for mesh parameters
	 *
	 * @param InMeshParams - A composite structure of the mobile mesh parameters needed by GL shaders
	 */
	void SetMobileMeshPixelParams(const FMobileMeshPixelParams& InMeshParams);

	/**
	 * Set the active and bound texture
	 *
	 * @param TextureUnit The unit to bind the texture to
	 * @param TextureName
	 * @param TextureType
	 * @param TextureFormat
	 */
	void SetActiveAndBoundTexture(UINT TextureUnit, UINT TextureName, UINT TextureType, UINT TextureFormat);

	/**
	 * Tells the shader manager what format of texture is in use in various texture units
	 *
	 * @param TextureUnit Which unit to assign a format to
	 * @param Format Format of the texture bound to TextureUnit
	 */
	void SetTextureFormat(UINT TextureUnit, EPixelFormat Format);

	/**
	 * @return the versioned parameter information for the given parameter slot
	 */
	struct FVersionedShaderParameter& GetVersionedParameter(INT Slot);
	
	
	/**
	 * Set the fact that the next SetSamplerState will reset having a lightmap texture to FALSE 
	 */
	void ResetHasLightmapOnNextSetSamplerState()
	{
		bWillResetHasLightmapOnNextSetSamplerState = TRUE;
	}
	
	/**
	 * @return whether or not a lightmap has been set
	 */
	UBOOL HasHadLightmapSet()
	{
		return bHasHadLightmapSet; 
	}

	/**
	 * @return whether or not a directional lightmap has been set
	 */
	UBOOL HasHadDirectionalLightmapSet()
	{
		return bHasHadLightmapSet && bHasHadDirectionalLightmapSet;
	}

	/**
	 * Set that the next call will be a global shader quad
	 */
	void SetNextDrawGlobalShader( EMobileGlobalShaderType GlobalShaderType )
	{
		NextDrawGlobalShaderType = GlobalShaderType;
	}

	/**
	 * @return Global shader type if the next draw call uses one, or EGST_None if it's not
	 */
	EMobileGlobalShaderType GetNextDrawGlobalShaderAndReset()
	{
		EMobileGlobalShaderType RetVal = NextDrawGlobalShaderType;
		NextDrawGlobalShaderType = EGST_None;
		return RetVal;
	}

    	/**
	 * @return Global shader type if the next draw call uses one, or EGST_None if it's not
	 */
	EMobileGlobalShaderType GetNextDrawGlobalShader()
	{
		return NextDrawGlobalShaderType;
	}

	/**
	 * @return whether or not the next draw should only render depth
	 */
	UBOOL IsDepthOnly() const
	{
		return GMobileRenderingDepthOnly;
	}

	/**
	 * @return whether or not the next draw should only render shadow depth (linear and biased)
	 */
	UBOOL IsShadowDepthOnly() const
	{
		return GMobileRenderingShadowDepth;
	}

	/**
	 * @return whether or not the next draw should only render shadow projection
	 */
	UBOOL IsForwardShadowProjection() const
	{
		return GMobileRenderingForwardShadowProjections;
	}

	/**
	 * @return whether or not gradient fog is allow by configuration
	 */
	UBOOL IsGradientFogAllowed() const
	{
		return (GSystemSettings.bAllowMobileFog && !GSystemSettings.bAllowMobileHeightFog);
	}

	/**
	 * @return whether or not fog should be enabled in general
	 */
	UBOOL IsGradientFogEnabledCommon() const
	{
		// If fog is enabled for this primitive and fog is not completely transparent
		return (IsGradientFogAllowed() && bIsFogEnabled && VertexSettings.bIsFogEnabledPerPrim && (FogColor.A > 0.0f) );
	}

	/**
	 * @return whether or not fog should be enabled for this primitive
	 */
	UBOOL IsGradientFogEnabled() const
	{
		if (GSystemSettings.bMobileMinimizeFogShaders)
		{
			return IsGradientFogAllowed();
		}
		else
		{
			// If fog is enabled for this primitive and the object is beyond the near fog volume
			return IsGradientFogEnabledCommon() && (ObjectDistance + ObjectBounds.SphereRadius >= FogStart);
		}
	}

	/**
	 * @return whether or not fog should be trivially, fully enabled for this primitive
	 */
	UBOOL IsFogSaturated() const
	{
		if (GSystemSettings.bMobileMinimizeFogShaders)
		{
			return FALSE;
		}
		else
		{
			return IsGradientFogEnabledCommon() && (ObjectDistance - ObjectBounds.SphereRadius >= FogEnd);
		}
	}
	
	/** @return whether height fog should be enabled for this primitive. */
	UBOOL IsHeightFogEnabled() const
	{
		return GSystemSettings.bAllowMobileFog && GSystemSettings.bAllowMobileHeightFog && bIsFogEnabled && VertexSettings.bIsFogEnabledPerPrim;
	}

	/**
	 * Send down fog settings to be used by the shader system 
	 * @param bInEnabled - Whether turn fog on or off
	 * @param InFogStart - Distance Fog begins at
	 * @param InFogStart - Distance Fog end at
	 * @param InFogColor - Final Fog color to fade to
	 */
	void SetFog (const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor)
	{
		bIsFogEnabled = bInEnabled;
		FogStart = InFogStart;
		FogEnd = InFogEnd;
		FogColor = InFogColor;
		
		// Pre-multiply the fog color for cases we trivially saturate
		FogColorAndAmount = FLinearColor(
			FogColor.R,
			FogColor.G,
			FogColor.B,
			FogColor.A
		);
	}

	/** Sets height-fog parameters to be used by the shader system. */
	void SetHeightFogParams(const FHeightFogParams& InHeightFogParams)
	{
		HeightFogParams = InHeightFogParams;
	}

	/**
	 * Checks whether color grading is enabled for mobile platforms during the forward rendering pass
	 *
	 * @return TRUE if color grading is enabled, false otherwise
	 */
	UBOOL IsMobileColorGradingEnabled () const
	{
		// When post-processing is enabled, we do color grading differently as part of PP
		return !GMobileAllowPostProcess && GSystemSettings.bAllowMobileColorGrading;
	}

	/** Sets the color grading parameters to be used by the shader system. */
	void SetMobileColorGradingParams (const FMobileColorGradingParams& Params)
	{
		ColorGradingParams = Params;
	}

	/**
	 * Send down bump offset settings to be used by the shader system 
	 * @param bInEnabled - Whether turn bump offset on or off
	 * @param InBumpEnd - Distance at which bump offset ends
	 */
	void SetBumpOffset (const UBOOL bInEnabled, const FLOAT InBumpEnd)
	{
		PixelSettings.bIsBumpOffsetEnabled = bInEnabled;
		BumpEnd = InBumpEnd;
	}

	/**
	 * Enables or disables gamma correction (SRGB reads and writes)
	 *
	 * @param	bInEnabled	True to enable gamma correction
	 */
	void SetGammaCorrection( const UBOOL bInEnabled )
	{
		bUseGammaCorrection = bInEnabled;
	}

	/**
	 * Returns true if gamma correction is turned on
	 *
	 * @return True if gamma correction is turned on
	 */
	UBOOL IsGammaCorrectionEnabled() const
	{
		return bUseGammaCorrection;
	}
	
	/**
	 * Used to show when a primitive is missing relevant stream data
	 *
	 * @param	bInEnabled	True to render the primitive component using our fall-back color(PINK)
	 */
	void SetToUseFallbackStreamColor( const UBOOL bInEnabled )
	{
		bUseFallbackColorStream = bInEnabled;
	}

	/**
	 * Returns true if primitive is missing relevant stream data
	 *
	 * @return True if primitive is missing relevant stream data
	 */
	UBOOL IsUsingFallbackColorStream()
	{
		return bUseFallbackColorStream;
	}

	/** Override mobile texture transform w/ the given one */
	void SetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform);

	/**
	 * Next primitive is a distance field font 
	 * @Param Params - all the variables needed for distance field fonts
	 */
	void SetMobileDistanceFieldParams (const struct FMobileDistanceFieldParams& Params);

	/**
	 * Sets whether or not normal mapping is enabled
	 */
	void EnableNormalMapping( UBOOL bInEnable )
	{
		VertexSettings.bIsNormalMappingEnabled = bInEnable;
	}

	/**
	 * Checks to see if normal mapping is currently enabled
	 *
	 * @return TRUE if normal mapping is turned on
	 */
	UBOOL IsNormalMappingEnabled() const
	{
		return VertexSettings.bIsNormalMappingEnabled && GSystemSettings.bAllowMobileNormalMapping;
	}


	/**
	 * Sets whether or not environment mapping is enabled
	 */
	void EnableEnvironmentMapping( UBOOL bInEnable )
	{
		VertexSettings.bIsEnvironmentMappingEnabled = bInEnable;
	}

	/**
	 * Checks to see if environment mapping is currently enabled
	 *
	 * @return TRUE if environment mapping is turned on
	 */
	UBOOL IsEnvironmentMappingEnabled() const
	{
		return VertexSettings.bIsEnvironmentMappingEnabled && GSystemSettings.bAllowMobileEnvMapping;
	}



	/**
	 * Sets whether or not rim lighting is enabled
	 */
	void EnableRimLighting( UBOOL bInEnable )
	{
		VertexSettings.bIsRimLightingEnabled = bInEnable;
	}

	/**
	 * Checks to see if rim lighting is currently enabled
	 *
	 * @return TRUE if rim lighting is turned on
	 */
	UBOOL IsRimLightingEnabled() const
	{
		return VertexSettings.bIsRimLightingEnabled && GSystemSettings.bAllowMobileRimLighting;
	}



	/**
	 * Sets whether or not specular is enabled
	 */
	void EnableSpecular( UBOOL bInEnable )
	{
		VertexSettings.bIsSpecularEnabled = bInEnable;
	}

	/**
	 * Checks to see if specular is currently enabled
	 *
	 * @return TRUE if specular is turned on
	 */
	UBOOL IsSpecularEnabled() const
	{
		return VertexSettings.bIsSpecularEnabled && GSystemSettings.bAllowMobileSpecular;
	}



	/**
	 * Sets whether or not per-pixel specular is enabled
	 */
	void EnablePixelSpecular( UBOOL bInEnable )
	{
		VertexSettings.bIsPixelSpecularEnabled = bInEnable;
	}

	/**
	 * Checks to see if pixel specular is currently enabled
	 *
	 * @return TRUE if pixel specular is turned on
	 */
	UBOOL IsPixelSpecularEnabled() const
	{
		return VertexSettings.bIsPixelSpecularEnabled && GSystemSettings.bAllowMobileSpecular;
	}


	/**
	 * Sets opacity source
	 */
	void SetOpacitySource( EMobileAlphaValueSource AlphaValueSource )
	{
		PixelSettings.AlphaValueSource = AlphaValueSource;
	}

	/**
	 * Sets whether or not bump offset is enabled
	 */
	void EnableBumpOffset( UBOOL bInEnable )
	{
		PixelSettings.bIsBumpOffsetEnabled = bInEnable;
	}

	/**
	 * Checks to see if bump offset is currently enabled
	 *
	 * @return TRUE if bump offset is turned on
	 */
	UBOOL IsBumpOffsetEnabled() const
	{
		// Note that this object distance check requires we set the bump uniforms *after* the mesh params are set
		return (PixelSettings.bIsBumpOffsetEnabled && GSystemSettings.bAllowMobileBumpOffset && GMobileDeviceAllowBumpOffset
#if !MINIMIZE_ES2_SHADERS
			&& FPointBoxIntersection(CameraPosition, ObjectBounds.GetBox().ExpandBy(BumpEnd))
#endif
			);
	}

	/**
	 * Sets whether or not wave-like vertex movement is enabled
	 */
	void EnableWaveVertexMovement( UBOOL bInEnable )
	{
		VertexSettings.bIsWaveVertexMovementEnabled = bInEnable;
	}

	/**
	 * Checks to see if wave-like vertex movement is enabled (Tree movement)
	 * @return TRUE if texture blending is turned on
	 */
	UBOOL IsWaveVertexMovementEnabled() const
	{
		return VertexSettings.bIsWaveVertexMovementEnabled && GSystemSettings.bAllowMobileVertexMovement;
	}


	/**
	 * Sets whether to use color fading along with the color and opacity
	 *
	 * @param	bInEnable			Whether to enable color fading
	 * @param	InColorAndAmount	RGB stores the fade color, A stores the fade progress
	 */
	void SetColorFading( const UBOOL bInEnable, const FLinearColor& InColorAndAmount )
	{
		// Store the color and pre-multiply the opacity
		FadeColorAndAmount.R = InColorAndAmount.R;
		FadeColorAndAmount.G = InColorAndAmount.G;
		FadeColorAndAmount.B = InColorAndAmount.B;
		FadeColorAndAmount.A = (bInEnable ? InColorAndAmount.A : 0.0f);
	}

	/**
	 * Handles updates to the viewport
	 *
	 * @param	PosX	Origin X
	 * @param	PosY	Origin Y
	 * @param	SizeX	Width
	 * @param	SizeY	Height
	 */
	void SetViewport( const UINT PosX, const UINT PosY, const UINT SizeX, const UINT SizeY );

	/** Resets all vertex factory flags */
	void ClearVertexFactoryFlags (void)
	{
		VertexFactoryFlags = 0;
	}

	void SetVertexFactoryFlags ( const DWORD InNewVertexFactoryFlags)
	{
		VertexFactoryFlags |= InNewVertexFactoryFlags;
	}

	/** Returns the amount of full screen color fading*/
	FLOAT GetMobilePercentColorFade (void) const
	{
		return FadeColorAndAmount.A;
	}

	const FMatrix& GetViewProjectionMatrix (void) const
	{
		return CachedViewProjectionMatrix;
	}

	void SetViewProjectionMatrix (const FMatrix& InMatrix)
	{
		CachedViewProjectionMatrix = InMatrix;
	}


	/** Sets the new upper sky light color */
	void SetUpperSkyColor( const FLinearColor& InColor )
	{
		UpperSkyColor = InColor;
	}

	/** Sets the new lower sky light color */
	void SetLowerSkyColor( const FLinearColor& InColor )
	{
		LowerSkyColor = InColor;
	}


	/** 
	 * Returns TRUE if per prim tracking is off, or we are currently on the proper primitive
	 * @return - TRUE if we should "fully" process this primitive (collect stats and actually draw)
	 */
	UBOOL IsCurrentPrimitiveTracked() const
	{
#if !(FINAL_RELEASE || SHIPPING_PC_GAME) || FINAL_RELEASE_DEBUGCONSOLE
		if (TrackedPrimitiveIndex == -1)
		{
			return TRUE;
		}

		switch(TrackedPrimitiveMode)
		{
		case TPM_Isolate: return TrackedPrimitiveIndex == CurrentPrimitiveIndex;
		case TPM_To:      return TrackedPrimitiveIndex >= CurrentPrimitiveIndex;
		case TPM_From:    return TrackedPrimitiveIndex <= CurrentPrimitiveIndex;
		default:          return TRUE;
		}
#else
		return TRUE;
#endif
	}

	/** 
	 * Reset to the 0th prim for a new round of stat tracking
	 */
	void NewFrame();

	/** 
	 * Increment the index that tracks what primitive we are currently rendering
	 */
	void NextPrimitive()
	{
		CurrentPrimitiveIndex++;
		// Reset the potential texture transform override
		bIsTextureCoordinateTransformOverriden = FALSE;
		bIsDistanceFieldFont = FALSE;
	}

	/** 
	 * Reset rendering to no longer track (and only render) one particular primitive
	 */
	void ResetTrackedPrimitive()
	{
		TrackedPrimitiveIndex = -1;
	}

	/**
	 * Cycle how the tracked primitive is rendered.  Currently there are three modes:
	 *   TPM_Isolated - Only renders the tracked primitive
	 *   TPM_To       - Renders all primitives up to and including the tracked primitive
	 *   TPM_From     - Renders all primitives from and including the tracked primitive
	 */
	void CycleTrackedPrimitiveMode()
	{
		TrackedPrimitiveMode = static_cast<ETrackedPrimitiveMode>((TrackedPrimitiveMode + 1) % TPM_Count);
	}

	/**
	 * Changes the currently tracked primitive by a delta and wraps around on both sides
	 */
	void ChangeTrackedPrimitive(const INT InDelta)
	{
		TrackedPrimitiveDelta = InDelta;
	}

	/**
	 * Prints a list of shader keys which were requested in the preprocessed cached, but were not present
	 */
	void PrintMissingShaderKeys();

	/** For debugging, tracking what material is requesting a particular shader */
	const FString& GetCurrentMaterialName() const
	{
		return MaterialName;
	}

private:

	struct FVertexSettings
	{
		void Reset (void)
		{
			appMemzero( this, sizeof(FVertexSettings) );
		}
		/** True if lighting is currently enabled */
		UBOOL bIsLightingEnabled;

		/** Texture coordinate source for base texture */
		EMobileTexCoordsSource BaseTextureTexCoordsSource;

		/** Texture coordinate source for detail texture */
		EMobileTexCoordsSource DetailTextureTexCoordsSource;

		/** Texture coordinate source for mask texture */
		EMobileTexCoordsSource MaskTextureTexCoordsSource;

		/** Whether or not texture coordinate transforms are enabled */
		UBOOL bBaseTextureTransformed;
		UBOOL bEmissiveTextureTransformed;
		UBOOL bNormalTextureTransformed;
		UBOOL bMaskTextureTransformed;
		UBOOL bDetailTextureTransformed;
		UBOOL bEnvironmentTextureTransformed;

		/** Whether or not color texture blending is currently enabled */
		UBOOL bIsUsingOneDetailTexture;
		UBOOL bIsUsingTwoDetailTexture;
		UBOOL bIsUsingThreeDetailTexture;
		/** Whether to lock the use of the color texture blending or allow it be overriden */
		UBOOL bIsColorTextureBlendingLocked;
		/** Texture blend factor source for blending textures */
		EMobileTextureBlendFactorSource TextureBlendFactorSource;

		/** Whether emissive is enabled */
		UBOOL bIsEmissiveEnabled;
		/** Color source for emissive */
		EMobileEmissiveColorSource EmissiveColorSource;
		/** Mask source for emissive */
		EMobileValueSource EmissiveMaskSource;

		/** Whether normal mapping is currently enabled */
		UBOOL bIsNormalMappingEnabled;

		/** Whether or not environment mapping is currently enabled */
		UBOOL bIsEnvironmentMappingEnabled;
		/** Mask source for environment map */
		EMobileValueSource EnvironmentMaskSource;
		/** Environment map fresnel amount (0.0 = disabled) */
		FLOAT EnvironmentFresnelAmount;

		/** Whether or not rim lighting is currently enabled */
		UBOOL bIsRimLightingEnabled;

		/** Mask source for rim lighting */
		EMobileValueSource RimLightingMaskSource;

		/** Whether or not specular is currently enabled */
		UBOOL bIsSpecularEnabled;

		/** Whether or not per pixel specular is currently enabled */
		UBOOL bIsPixelSpecularEnabled;

		/** Enables ambient occlusion and sets the data source for this */
		EMobileAmbientOcclusionSource AmbientOcclusionSource;

		/** Whether or not wave vertex movement is enabled */
		UBOOL bIsWaveVertexMovementEnabled;

		/** Whether or not fog is enabled PER primitive */
		UBOOL bIsFogEnabledPerPrim;

		/**Multiply a constant into diffuse color*/
		UBOOL bUseUniformColorMultiply;

		/**Whether to use detail normal for mobile*/
		UBOOL bUseDetailNormal;

		/**Multiply Vertex Color into diffuse color*/
		UBOOL bUseVertexColorMultiply;

		/**Whether to use Monochrome Layer Blending for landscape */
		UBOOL bUseLandscapeMonochromeLayerBlending;
	};


	struct FPixelSettings
	{
		void Reset (void)
		{
			appMemzero( this, sizeof(FPixelSettings) );
		}

		/** Whether or not to use bump offset when rendering and the desired height ratio */
		UBOOL bIsBumpOffsetEnabled;

		/** Source of specular mask for per-vertex specular on mobile */
		EMobileAlphaValueSource AlphaValueSource;

		/** Source to use for Color Multiplication */
		EMobileColorMultiplySource ColorMultiplySource;

		/** Which specular mask mode to use for per vertex specular */
		EMobileSpecularMask SpecularMask;

		/** Environment map blend mode */
		EMobileEnvironmentBlendMode EnvironmentBlendMode;
	};

	struct FMeshSettings
	{
		void Reset (void)
		{
			appMemzero( this, sizeof(FMeshSettings) );
		}

		/** Sprite/SubUV screen alignment being used */
		ESpriteScreenAlignment ParticleScreenAlignment;
	};

	/** Made into a structure that can be reset for each SetMobileMaterialVertexParams */
	FVertexSettings VertexSettings;
	/** Made into a structure that can be reset for each SetMobileMaterialPixelParams */
	FPixelSettings PixelSettings;
	/** Made into a structure that can be reset for each SetMobileMeshVertexParams */
	FMeshSettings MeshSettings;

	/** Whether or not texture coordinate transforms are overridden*/
	UBOOL bIsTextureCoordinateTransformOverriden;
	/** Whether or not distance field fonts have been requested for this primitive*/
	UBOOL bIsDistanceFieldFont;

#if !FINAL_RELEASE
	/** Whether or not a new material has been set for this draw. */
	UBOOL bNewMaterialSet;
#endif

	/** For debugging, tracking what material is requesting a particular shader */
	FString MaterialName;

	/**
	 * Material/mesh shader state
	 */
	DWORD VertexFactoryFlags;

	/** Global shader type (PrimitiveType is EPT_GlobalShader) */
	EMobileGlobalShaderType GlobalShaderType;

	/**Settings NOT set per primitive- Do not reset between prims*/
	/** True if we're using color fading (lerp pixel shader output to specific color) */
	UBOOL bIsColorFadingEnabled;

	/** Since AlphaTest is a shader parameter, not a render state, we cache the value until draw time */
	EBlendMode BlendMode;
	
	/** If color fading is enabled, the color to fade to along with the fade progress scalar in the alpha */
	FLinearColor FadeColorAndAmount;

#if FLASH
	/** On Flash, emulate viewport functionality with a vertex shader contant and a scissor rect */
	FVector4 ViewportScaleBias;
#endif

	/** Whether or not fog is enabled at all */
	UBOOL bIsFogEnabled;
	/** Fog Start Distance */
	FLOAT FogStart;
	/** Fog End Distance */
	FLOAT FogEnd;
	/**FogColor*/
	FLinearColor FogColor;
	FLinearColor FogColorAndAmount;

	FHeightFogParams HeightFogParams;
	FMobileColorGradingParams ColorGradingParams;

	FLOAT BumpReferencePlane;
	FLOAT BumpHeightRatio;
	FLOAT BumpEnd;

	/** True if gamma correction (SRGB reads and writes) are enabled */
	UBOOL bUseGammaCorrection;

	UBOOL bUseFallbackColorStream;
	
	/** Whether to output which keys are being warmed */
	UBOOL bDebugShowWarmedKeys;

	//Temp storage for sway parameters that aren't used until mesh data is sent down
	FLOAT SwayTime;
	FLOAT SwayMaxAngle;

	/** Cached brightest directional light color */
	FLinearColor BrightestLightColor;

	/** Cached specular light color */
	FLinearColor SpecularColor;

	//Cached off view projection matrix for use on the CPU
	FMatrix CachedViewProjectionMatrix;

	/**
	 * Shader manager
	 */
	/** Set of our known programs */
	class FES2ShaderProgram *MaterialPrograms;
	class FES2ShaderProgram *GlobalPrograms;
	
	/** Set of the current state of all shader parameters (one entry for each param slot) */
	struct FVersionedShaderParameter* VersionedShaderParameters;

	/** Whether or not the next SetSamplerState call should reset if we have a lightmap texture set or not */
	UBOOL bWillResetHasLightmapOnNextSetSamplerState;
	
	/** Whether or not a lightmap texture has been set (which means we want to use texture lit shader) */
	UBOOL bHasHadLightmapSet;

	/** Whether or not a directional lightmap texture has been set (enable directional lightmaps) */
	UBOOL bHasHadDirectionalLightmapSet;

	/** Global shader type if the next draw call uses one, or EGST_None if it's not (needed for choosing shader) */
	EMobileGlobalShaderType NextDrawGlobalShaderType;

	/** Shared definitions for all shader files */
	FString CommonShaderPrefixFile;

	/** Shared definitions for all vertex shader files */
	FString VertexShaderPrefixFile;

	/** Shared definitions for all pixel shader files */
	FString PixelShaderPrefixFile;

	/** The position and distance of the object being drawn and bounds */
	FVector CameraPosition;
	FVector ObjectPosition;
	FLOAT   ObjectDistance;
	FBoxSphereBounds ObjectBounds;

	/** The current upper sky light color */
	FLinearColor UpperSkyColor;

	/** The current lower sky light color */
	FLinearColor LowerSkyColor;

	//For debugging each prim, index of the current primitive being rendered
	INT CurrentPrimitiveIndex;
	//For debugging each prim, the desired delta of the index to be set at the end of the frame
	INT TrackedPrimitiveDelta;
	//For debugging each prim, the index of the desired primitive to render exclusively (-1, the default, renders all primitives)
	INT TrackedPrimitiveIndex;
	//For debugging each prim, the mode indicates what is rendered
	ETrackedPrimitiveMode TrackedPrimitiveMode;

	/** Used to force on/off features depending on the system */
	EMobilePlatformFeatures PlatformFeatures;
	EMobilePrimitiveType PrimitiveType;

	/** A list of all the preprocessed shaders that were requested by a key but were missing from the cache */
	TArray<FString> MissingPreprocessedShaders;

	/** Shaders that have already been compiled. */
	TSet<FProgramKey> CompiledShaders;

	// allow the program class access to our goods
	friend class FES2ShaderProgram;
};

#if FLASH
enum RHIMSG
{
	RHI_glClear = 1,
	RHI_glClearColor,
	RHI_glFinish,
	RHI_glActiveTexture,
	RHI_glFlush,
	RHI_glBindTexture,
	RHI_glEnable,
	RHI_glDisable,
	RHI_glTexParameteri,
	RHI_glTexImage2D,
	RHI_glCompressedTexImage2D,
	RHI_glGenTextures,
	RHI_glDeleteTextures,
	RHI_glPixelStorei,
	RHI_glReadPixels,
	RHI_glGenBuffers,
	RHI_glDeleteBuffers,
	RHI_glGenFramebuffers,
	RHI_glGenRenderbuffers,
	RHI_glRenderbufferStorage,
	RHI_glBindBuffer,
	RHI_glBindFramebuffer,
	RHI_glFramebufferRenderbuffer,
	RHI_glFramebufferTexture2D,
	RHI_glBindRenderbuffer,
	RHI_glBufferData,
	RHI_glBufferSubData,
	RHI_glColorMask,
	RHI_glDepthMask,
	RHI_glStencilMask,
	RHI_glDepthFunc,
	RHI_glViewport,
	RHI_glScissor,
	RHI_glDepthRangef,
	RHI_glClearDepthf,
	RHI_glClearStencil,
	RHI_glFrontFace,
	RHI_glEnableVertexAttribArray,
	RHI_glDisableVertexAttribArray,
	RHI_glUniform1i,
	RHI_glUniform1fv,
	RHI_glUniform2fv,
	RHI_glUniform3fv,
	RHI_glUniform4fv,
	RHI_glUniformMatrix3fv,
	RHI_glUniformMatrix4fv,
	RHI_glDrawElements,
	RHI_glVertexAttribPointer,
	RHI_glUseProgram,
	RHI_glLinkProgram,
	RHI_glDeleteProgram,
	RHI_glCompileShader,
	RHI_glShaderSource,
	RHI_glAttachShader,
	RHI_glDetachShader,
	RHI_glDeleteShader,
	RHI_glCreateProgram,
	RHI_glCreateShader,
	RHI_glGetAttribLocation,
	RHI_glGetUniformLocation,
	RHI_glBlendColor,
	RHI_glBlendEquationSeparate,
	RHI_glBlendFuncSeparate,
	RHI_glStencilOpSeparate,
	RHI_glStencilFuncSeparate,
	RHI_eglSwapBuffers,
	RHI_glDrawArrays,
	RHI_glPolygonOffset,
};
#endif

/**
 * Gets rid of all those pesky resources
 */
void ClearES2PendingResources();

/**
 * A holder struct that combines color and depth render targets into a GL Frame Buffer Object (fbo)
 */
class FES2FrameBuffer
{
public:
	/**
	 * Constructor
	 */
	FES2FrameBuffer(FSurfaceRHIParamRef InColorRenderTarget, FSurfaceRHIParamRef InDepthRenderTarget);

	/** The color surface to render to */
	FSurfaceRHIParamRef ColorRenderTarget;

	/** The depth surface to render to */
	FSurfaceRHIParamRef DepthRenderTarget;

	/** GL fbo name */
	GLuint FBO;
};

/**
 * Manager class to track streams, buffers, etc
 */
class FES2RenderManager
{
	/** Max vertex streams we can send to a shader */
	static const UINT MaxVertexStreams = 16;

	/** How much memory to allocate for the scratch buffers */
	UINT VertexScratchBufferSize;
	UINT IndexScratchBufferSize;

	/** What value to align data to */
	static const UINT ScratchBufferAlignment = 16; 

	/**
	 * Helper struct to hold SetStreamSource info
	 */
	struct FES2PendingStream
	{
		FVertexBufferRHIRef VertexBuffer;
		UINT Stride;
        UINT Offset;
	};
	
public:

	/**
	 * Constructor
	 */
	FES2RenderManager()
		: bArePendingStreamsDirty(FALSE)
		, bProgramUpdateWasSuccessful(FALSE)
		, bAttributeUpdateWasSuccessful(FALSE)
		, bAttributesAndProgramsAreValid(FALSE)
		, AttribMask(0)
	{
	}

#if !FLASH
	static class FSocket* Socket;
#endif
#if FLASH
	static UBOOL ConnectRHIProxy();

    static UBOOL RHISendMessage(RHIMSG msg);
    static UBOOL RHISendMessage(RHIMSG msg, float f1, float f2, float f3, float f4);
    static UBOOL RHISendMessage(RHIMSG msg, float f1);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, const char *s1);
    static UBOOL RHISendMessage(RHIMSG msg, const uint8_t *bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, const uint8_t *bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, bool b2, const uint8_t *bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, const uint8_t *bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3, const uint8_t *bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, bool v1, bool v2, bool v3, bool v4);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, const uint8_t* bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, const uint8_t* bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3, bool v4, uint32_t v5, const uint8_t* bytes, uint32_t len);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4, bool v5, uint32_t v6, uint32_t v7);
    static UBOOL RHISendMessage(RHIMSG msg, uint32_t v1, uint32_t v2, uint32_t v3, bool v4, uint32_t v5, uint32_t v6);
    
    static UBOOL RHIGetBytes(uint8_t* data);
    static int RHIGetUInt();

	static UBOOL RHISendUINT(const unsigned int value);
	static UBOOL RHISendINT(const int value);
	static UBOOL RHISendBOOL(const unsigned char value);
	static UBOOL RHISendSTRING(const char * name);
	static UBOOL RHISendSTRINGLIST(UINT count, const char ** strings);
	static UBOOL RHISendFLOAT(const float value);
	static UBOOL RHISendBYTES(UINT size, const void* data);
	static UBOOL RHISendSHORTBYTES(UINT size, const void* data);

	static void RHIglClear(UINT);
	static void RHIglClearColor(FLOAT,FLOAT,FLOAT,FLOAT);
	static void RHIglFinish();
	static void RHIglActiveTexture(UINT);
	static void RHIglFlush();
	static void RHIglBindTexture(UINT, UINT);
	static void RHIglEnable(UINT);
	static void RHIglDisable(UINT);
	static void RHIglTexParameteri(UINT,UINT,INT);
	static void RHIglTexImage2D(UINT,INT,INT,INT,INT,INT,UINT,UINT, const void*);
	static void RHIglCompressedTexImage2D(UINT,INT,UINT,INT,INT,INT,INT, const void*);
	static void RHIglGenTextures(INT, UINT*);
	static void RHIglDeleteTextures(INT, UINT*);
	static void RHIglPixelStorei(UINT, INT);
	static void RHIglReadPixels(INT, INT, INT, INT, UINT, UINT, void*);
	static void RHIglGenBuffers(INT, UINT*);
	static int RHIglGetIntegerv(INT, INT*);
	static void RHIglDeleteBuffers(INT, UINT*);
	static void RHIglGenFramebuffers(INT, UINT*);
	static void RHIglGenRenderbuffers(INT, UINT*);
	static void RHIglDeleteRenderbuffers(INT, UINT*);
	static void RHIglRenderbufferStorage(UINT, UINT, INT, INT);
	static void RHIglBindBuffer(UINT, UINT);
	static void RHIglBindFramebuffer(UINT, UINT);
	static void RHIglDeleteFramebuffers(INT, UINT*);
	static void RHIglFramebufferRenderbuffer(UINT, UINT, UINT, UINT);
	static void RHIglFramebufferTexture2D(UINT, UINT, UINT, UINT, INT);
	static void RHIglBindRenderbuffer(UINT, UINT);
	static void RHIglBufferData(UINT,UINT,const void*,UINT);
	static void RHIglBufferSubData(UINT,UINT,UINT,const void*);
	static void RHIglDepthMask(unsigned char);
	static void RHIglStencilMask(UINT);
	static void RHIglStencilFunc(UINT,UINT,UINT);
	static void RHIglStencilOp(UINT,UINT,UINT);
	static void RHIglColorMask(unsigned char, unsigned char, unsigned char, unsigned char);
	static void RHIglDepthFunc(UINT);
	static void RHIglViewport(UINT,UINT,UINT,UINT);
	static void RHIglScissor(UINT,UINT,UINT,UINT);
	static void RHIglDepthRangef(FLOAT,FLOAT);
	static void RHIglClearDepthf(FLOAT);
	static void RHIglFrontFace(UINT);
	static void RHIglClearStencil(INT);
	static void RHIglEnableVertexAttribArray(UINT);
	static void RHIglDisableVertexAttribArray(UINT);
	static void RHIglUniform1i(INT,INT);
	static void RHIglUniform1fv(INT,INT,const void*);
	static void RHIglUniform2fv(INT,INT,const void*);
	static void RHIglUniform3fv(INT,INT,const void*);
	static void RHIglUniform4fv(INT,INT,const void*);
	static void RHIglUniformMatrix3fv(INT,INT,unsigned char,const void*);
	static void RHIglUniformMatrix4fv(INT,INT,unsigned char,const void*);
	static void RHIglDrawElements(UINT,INT,UINT,const void*);
	static void RHIglVertexAttribPointer(UINT,INT,UINT,unsigned char,INT, const void*, UINT);
	static void RHIglUseProgram(UINT);
	static void RHIglLinkProgram(UINT);
	static void RHIglDeleteProgram(UINT);
	static void RHIglCompileShader(UINT);
	static void RHIglShaderSource(UINT,INT,const char ** string,void*);
	static void RHIglAttachShader(UINT,UINT);
	static void RHIglDetachShader(UINT,UINT);
	static void RHIglDeleteShader(UINT);
	static int RHIglCreateProgram();
	static int RHIglCreateShader(UINT);
	static int RHIglGetAttribLocation(UINT, const char * name);
	static int RHIglGetUniformLocation(UINT, const char * name);
	static void RHIglBlendColor(FLOAT,FLOAT,FLOAT,FLOAT);
	static void RHIglBlendEquationSeparate(UINT,UINT);
	static void RHIglBlendFuncSeparate(UINT,UINT,UINT,UINT);
	static void RHIglStencilOpSeparate(UINT,UINT,UINT,UINT);
	static void RHIglStencilFuncSeparate(UINT,UINT,UINT,UINT);
	static void RHIeglSwapBuffers();
	static void RHIglDrawArrays(UINT,UINT,UINT);
	static void RHIglPolygonOffset(FLOAT,FLOAT);
#endif
	/**
	 *	Initialize the render manager
	 */
	void InitRHI();


	/**
	 * Called when shutting down the RHI
	 */
	void ExitRHI();

	/** 
	 * Clears out any GPU Resources used by Shader Manager
	 */
	void ClearGPUResources();

	/** 
	 * Perform whatever is required when starting a new frame
	 */
	void NewFrame()
	{
#if _WINDOWS
		// Toggle which debugging VBO we'll use for NULL Color Streams
		GNullColorVBOIndex = (GNullColorVBOIndex + 1) % 2;
#endif
	}

	/**
	 * Sets a stream input from the engine
	 *
	 * @param StreamIndex Which stream is being set
	 * @param VertexBuffer The vertex buffer that is the source of the stream
	 * @param Stride Size of one vertex in the vertex buffer
	 */
	void SetStream(UINT StreamIndex, FVertexBufferRHIParamRef VertexBuffer, UINT Stride, UINT Offset)
	{
		// remember the stream information
		PendingStreams[StreamIndex].VertexBuffer = VertexBuffer;
		PendingStreams[StreamIndex].Stride = Stride;
        PendingStreams[StreamIndex].Offset = Offset;
		
		bArePendingStreamsDirty = TRUE;
	}

	/**
	 * Set the vertex declaration for the upcomfing draw call
	 *
	 * @param VertexDeclaration The declaration describing vertex format
	 */
	void SetVertexDeclaration(FVertexDeclarationRHIRef VertexDeclaration)
	{
		PendingVertexDeclaration = VertexDeclaration;
		bArePendingStreamsDirty = TRUE;
	}
	
	/**
	 * Set the vertex attribute inputs, and set the current program
	 *
	 * @param InPrimitiveData For DrawPrimUP, this is the immediate vertex data
	 * @param InVertexStride The size of each vertex in InPrimitiveData, if given
	 * @param InVertexDataSize The size of the vertex data in bytes.
	 */
	UBOOL UpdateAttributesAndProgram(const void* InPrimitiveData=NULL, INT InVertexStride=-1, UINT InVertexDataSize=0);
	
	/**
	 * Set Active Vertex Attributes and corresponding mask
	 *
	 * @param NewAttribMask 
	 */
	void PrepareAttributes( UINT NewAttribMask );

	/**
	 * Retrieve scratch memory for dynamic draw data
	 *
	 * @param VertexDataSize Size in bytes of vertex data we need
	 *
	 * @return Pointer to start of properly aligned data
	 */
	BYTE* AllocateVertexData(UINT VertexDataSize);
	void DeallocateVertexData();

	/**
	 * Retrieve scratch memory for dynamic draw data
	 *
	 * @param IndexDataSize Size in bytes of index data we need
	 *
	 * @return Pointer to start of properly aligned data
	 */
	BYTE* AllocateIndexData(UINT IndexDataSize);
	void DeallocateIndexData();

	/**
	 * Remember values needed during RHIEndDraw[Indexed]PrimitiveUP
	 */
	void CacheUPValues(UINT PrimitiveType, UINT VertexStride, UINT NumPrimitives, void* VertexData, void* IndexData=NULL, UINT VertexDataSize=0)
	{
		CachedPrimitiveType = PrimitiveType;
		CachedVertexStride = VertexStride;
		CachedNumPrimitives = NumPrimitives;
		CachedVertexData = VertexData;
		CachedIndexData = IndexData;
        CachedVertexDataSize = VertexDataSize;
	}
	
	/**
	 * Retreive cached values
	 */
	FORCEINLINE UINT GetCachedPrimitiveType() { return CachedPrimitiveType; }
	FORCEINLINE UINT GetCachedVertexStride() { return CachedVertexStride; }
	FORCEINLINE UINT GetCachedNumPrimitives() { return CachedNumPrimitives; }
	FORCEINLINE void* GetCachedVertexData() { return CachedVertexData; }
	FORCEINLINE void* GetCachedIndexData() { return CachedIndexData; }
	FORCEINLINE UINT GetCachedVertexDataSize() { return CachedVertexDataSize; }

	// A map of all the known framebuffers
	TMap<DWORD, FES2FrameBuffer> FrameBuffers;

	FES2FrameBuffer* FindOrCreateFrameBuffer(FSurfaceRHIParamRef NewRenderTargetRHI,FSurfaceRHIParamRef NewDepthStencilTargetRHI);
	void RemoveFrameBufferReference(FSurfaceRHIParamRef RenderTargetRHI);

	void ResetAttribMask(void) { AttribMask = 0; }

private:

	/** Pointer to scratch buffers for vertex and index data */
	BYTE* VertexScratchBuffer;
	BYTE* IndexScratchBuffer;

	/** Simple refcount used to assert that only one allocation is active at a time */
	UINT VertexScratchBufferRefcount;
	UINT IndexScratchBufferRefcount;
	
	/** An array of streams that have been set by the engine, but we can't hook up to GL until draw time (also need the VertexDeclaration) */
	FES2PendingStream PendingStreams[MaxVertexStreams];
	
	/** Set to TRUE when any of GPendingStreams have been modified */
	UBOOL bArePendingStreamsDirty;

	/** Set to TRUE after a successful update of attributes and programs */
	UBOOL bProgramUpdateWasSuccessful;
	UBOOL bAttributeUpdateWasSuccessful;
	UBOOL bAttributesAndProgramsAreValid;

	/** Variables used for visual debugging of issues rising from requiring a vertex color stream, but not having one */
	INT GNullColorVBOIndex;
	GLuint GNullColorVBOs[2];

	GLuint GNullWeightVBO;

	/** Vertex format declaration, which has to be cached until draw time (to combine with the GPendingStreams) */
	FVertexDeclarationRHIRef PendingVertexDeclaration;

	/** What arrays are currently enabled */
	UINT AttribMask;
	
	/** Cached values for using during RHIEndDraw[Indexed]PrimitiveUP */
	UINT CachedPrimitiveType;
	UINT CachedVertexStride;
	UINT CachedNumPrimitives;
    UINT CachedVertexDataSize;
	void* CachedVertexData;
	void* CachedIndexData;
};


/**
 * ES2 rendering core
 */
class FES2Core
{

public:

	/**
	 * Initializes the ES2 rendering system
	 */
	static void InitES2Core();

	/**
	 * Shuts down the ES2 renderer
	 */
	static void DestroyES2Core();

	/**
	 * Called when a new viewport is created
	 *
	 * @param	Viewport	The newly created viewport
	 * @param	NativeWindowHandle Native window handle, retrieved from the FViewport::GetWindow()
	 */
	static void OnViewportCreated(FES2Viewport* Viewport, void* NativeWindowHandle);

	/**
	 * Called when a new viewport is destroyed
	 *
	 * @param	Viewport	The viewport that's currently being destroyed
	 */
	static void OnViewportDestroyed(FES2Viewport* Viewport);

	/**
	 * Called when a viewport needs to present it's back buffer
	 */
	static void SwapBuffers(FES2Viewport* Viewport=NULL);

	/**
	 * Called when a viewport needs to present it's back buffer
	 */
	static void MakeCurrent(FES2Viewport* Viewport=NULL);

	/**
	 * Called when a viewport needs to present it's back buffer
	 */
	static void UnmakeCurrent(FES2Viewport* Viewport=NULL);

private:

	/**
	 * Checks the OpenGL extensions to see what the platform supports.
	 */
	static void SetupPlatformExtensions();

	/** The ES2 viewports currently initialized */
	static TArray<FES2Viewport*> ActiveViewports;

	/** The most recent viewport that was MakeCurrent'd */
	static FES2Viewport* CurrentViewport;
};
////////////////////////////////////
//
// Globals
// 
////////////////////////////////////

/**
 * Shader manager instance
 */
extern FES2ShaderManager GShaderManager;

/**
 * Single instance for the rendering manager to track streams, buffers, etc
 */
extern FES2RenderManager GRenderManager;

/**
 * Map the Unreal semantic to a hardcoded location that we will
 * bind it to in GL.
 */
DWORD TranslateUnrealUsageToBindLocation(DWORD Usage);

#endif // WITH_ES2_RHI

#endif // __ES2RHIPRIVATE_H__
