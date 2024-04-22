/*=============================================================================
	Scene.h: Scene manager definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Includes the draw mesh macros
#include "UnSceneUtils.h"

// enums are already declared in Engine.h
#define NO_ENUMS 1
#include "EngineSceneClasses.h"
#undef NO_ENUMS

// Forward declarations.
class FSceneViewFamily;
class FSceneInterface;
class FLightSceneInfo;
class ULightComponent;
class USceneCaptureComponent;
class FCaptureSceneInfo;
class UPostProcessChain;

//
//	EShowFlags
//

#if CONSOLE && FINAL_RELEASE

#define MAKE_SHOW_FLAG(BitNum)                           (1<<(BitNum-1))
#define MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(BitNum)  SHOW_RESERVED_FLAG
#define MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(BitNum) 0

#else

#define MAKE_SHOW_FLAG(BitNum)                           EShowFlags(E_ForceInit, BitNum)
#define MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(BitNum)  EShowFlags(E_ForceInit, BitNum)
#define MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(BitNum) EShowFlags(E_ForceInit, BitNum)

#endif

//Reserved for "fall through" logic in shipping
#define	SHOW_RESERVED_FLAG			MAKE_SHOW_FLAG(1)	// Defines to 0x01 which let's the compiler opt out of always true compares (See EShowFlags)

//flags that should always be tested for shipping (scene captures)
#define	SHOW_Lighting				MAKE_SHOW_FLAG(2)
#define	SHOW_SceneCaptureUpdates	MAKE_SHOW_FLAG(3)	// Update scene capture probes
#define	SHOW_DynamicShadows			MAKE_SHOW_FLAG(4)	// Draw dynamic shadows.
#define	SHOW_Fog					MAKE_SHOW_FLAG(5)
#define	SHOW_PostProcess			MAKE_SHOW_FLAG(6)	// Draw post process effects
#define	SHOW_Sprites				MAKE_SHOW_FLAG(7)	// Draw sprite components
#define	SHOW_LightShafts			MAKE_SHOW_FLAG(8)	// Post processing: Light shafts

//flags that should always set for shipping
#define	SHOW_Decals					MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(32)	// Draw decals.
#define	SHOW_InstancedStaticMeshes	MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(33)	// Show instanced static meshes
#define	SHOW_StaticMeshes			MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(34)
#define	SHOW_Terrain				MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(35)
#define	SHOW_BSPTriangles			MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(36)	// Draws BSP triangles
#define	SHOW_SkeletalMeshes 		MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(37)
#define	SHOW_SpeedTrees				MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(38)	// Renders SpeedTrees
#define	SHOW_LensFlares				MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(39)	// Renders LensFlares
#define	SHOW_LOD					MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(40)	// Use LOD parenting, MinDrawDistance, etc. If disabled, will show LOD parenting lines
#define	SHOW_Game					MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(41)
#define	SHOW_CameraInterpolation	MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(42)	// to disable motion blur and AO history smoothing for the editor camera, is set for the game
#define	SHOW_Particles				MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(43)	// Draws particles
#define	SHOW_BSP					MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(44)
#define	SHOW_Materials				MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(45)
#define	SHOW_MotionBlur				MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(46)	// Post processing: MotionBlur (also disabled velocity rendering)
#define	SHOW_ImageGrain				MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(47)	// Post processing: ImageGrain
#define	SHOW_DepthOfField			MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(48)	// Post processing: Depth of Field
#define	SHOW_ImageReflections		MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(49)	// Post processing: Image Reflections
#define	SHOW_SubsurfaceScattering	MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(50)	// Post processing: Subsurface Scattering
#define	SHOW_LightFunctions			MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(51)	// Post processing: Light Functions
#define	SHOW_Tessellation			MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(52)	// Post processing: Tessellation
#define	SHOW_UnlitTranslucency		MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(53)	// Render unlit translucency
#define	SHOW_TranslucencyDoF		MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(54)  // Render translucency blur factor to translucency DoF blur buffer
#define	SHOW_SSAO					MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(55)	// Post processing: Screen Space Ambient Occlusion

//flags that should never be set for shipping
#define	SHOW_DecalInfo				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(64)	// Draw decal dev info (frustums, tangent axes, etc).
#define	SHOW_LightRadius			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(65)	// Draw point light radii.
#define	SHOW_AudioRadius			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(66)	// Draw sound actor radii.
#define	SHOW_Wireframe				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(67)
#define	SHOW_LightComplexity		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(68)
#define	SHOW_Brushes				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(69)
// unused							MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(70)
#define	SHOW_LevelColoration		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(71)	// Render objects with colors based on what the level they belong to.
#define	SHOW_BSPSplit				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(72)	// Colors BSP based on model component association.
#define	SHOW_CollisionNonZeroExtent	MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(73)	
#define	SHOW_CollisionZeroExtent	MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(74)	
#define	SHOW_CollisionRigidBody		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(75)	
#define	SHOW_PropertyColoration		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(76)	// Render objects with colors based on the property values.
#define	SHOW_StreamingBounds		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(77)	// Render streaming bounding volumes for the currently selected texture
#define	SHOW_TextureDensity			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(78)	// Colored according to world-space texture density.
#define	SHOW_ShaderComplexity		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(79)	// Renders world colored by shader complexity
#define	SHOW_LightMapDensity		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(80)	// Render checkerboard material with UVs scaled by lightmap resolution w. color tint for world-space lightmap density
#define	SHOW_SentinelStats			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(81)	// Render stats from Sentinel stats viewer in editor
#define	SHOW_Splines				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(82)	
#define	SHOW_VertexColors			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(83)	// Vertex colors
#define	SHOW_Editor					MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(84)	
#define	SHOW_Collision				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(85)	
#define	SHOW_Grid					MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(86)	
#define	SHOW_Selection				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(87)	
#define	SHOW_Bounds					MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(88)	
#define	SHOW_Constraints			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(89)	
#define	SHOW_Paths					MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(90)	// Draws BSP brushes.
#define	SHOW_MeshEdges				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(91)	// In the filled view modes, render mesh edges as well as the filled surfaces.
#define	SHOW_LargeVertices			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(92)	// Displays large clickable icons on static mesh vertices
#define	SHOW_HitProxies				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(93)	// Draws each hit proxy in the scene with a different color.
#define	SHOW_ShadowFrustums			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(94)	// Draws un-occluded shadow frustums as 
#define	SHOW_ModeWidgets			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(95)	// Draws mode specific widgets and controls in the viewports (should only be set on viewport clients that are editing the level itself)
#define	SHOW_KismetRefs				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(96)	// Draws green boxes around actors in level which are referenced by Kismet. Only works in editor.
#define	SHOW_Volumes				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(97)	// Draws Volumes
#define	SHOW_CamFrustums			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(98)	// Draws camera frustums
#define	SHOW_NavigationNodes		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(99)	// Draws actors associated with path noding
#define	SHOW_LightInfluences		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(100)	// Visualize light influences
#define	SHOW_BuilderBrush			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(101)	// Draws the builder brush wireframe
#define	SHOW_TerrainPatches			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(102)	// Draws an outline around each terrain patch
#define	SHOW_Cover					MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(103)	// Complex cover rendering
#define	SHOW_ActorTags				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(104)	// Draw an Actors Tag next to it in the viewport. Only works in the editor.
#define	SHOW_VisualizeDOFLayers		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(105)	// Post processing: Visualize Depth of Field Layers
#define	SHOW_PreShadowFrustums		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(106)	// Draws un-occluded preshadow frustums as wireframe
#define	SHOW_TemporalAA				MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(107)	// Post processing: Temporal AA
#define	SHOW_PreShadowCasters		MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(108)	// Draws the meshes that are used to cast preshadows
#define	SHOW_VisualizeSSAO			MAKE_SHOW_FLAG_ALWAYS_FALSE_FOR_SHIPPING(109)	// Post processing: Visualize Screen Space Ambient Occlusion

#define	SHOW_PostProcessAA			MAKE_SHOW_FLAG_ALWAYS_TRUE_FOR_SHIPPING(110)	// Post processing: PostProcessAA

// more flags can be added to the end, the number can go up to 127 (see MAKE_SHOW_FLAG)


/*
 * The following code declares combined EShowFlag constants.
 * The actual masks are defined in Scene.cpp (we don't use #define to avoid the run-time cost of generating the bitmask).
 */

extern EShowFlags SHOW_Collision_Any;
extern EShowFlags SHOW_ViewMode_Lit;
extern EShowFlags SHOW_DefaultGame;
extern EShowFlags SHOW_DefaultEditor;

// The show flags which only editor views can use.
extern EShowFlags SHOW_EditorOnly_Mask;

/**
 * View modes, predefined configurations of a subset of the show flags.
 */

// The flags to clear before setting any of the viewmode show flags.
extern EShowFlags SHOW_ViewMode_Mask;
// Wireframe w/ BSP
extern EShowFlags SHOW_ViewMode_Wireframe;
// Wireframe w/ brushes
extern EShowFlags SHOW_ViewMode_BrushWireframe;
// Unlit
extern EShowFlags SHOW_ViewMode_Unlit;
// Lit wo/ materials
extern EShowFlags SHOW_ViewMode_LightingOnly;
// Colored according to light count.
extern EShowFlags SHOW_ViewMode_LightComplexity;
// Colored according to shader complexity.
extern EShowFlags SHOW_ViewMode_ShaderComplexity;
// Colored according to world-space texture density.
extern EShowFlags SHOW_ViewMode_TextureDensity;
// Colored according to world-space LightMap texture density.
extern EShowFlags SHOW_ViewMode_LightMapDensity;
// Colored according to light count - showing lightmap texel density on texture mapped objects.
extern EShowFlags SHOW_ViewMode_LitLightmapDensity;

// -----------------------------------------------------------------------------

/**
 * The scene manager's persistent view state.
 */
class FSceneViewStateInterface
{
public:
	FSceneViewStateInterface()
		:	ViewParent( NULL )
		,	NumChildren( 0 )
	{}
	
	/** Called in the game thread to destroy the view state. */
	virtual void Destroy() = 0;

public:
	/** Sets the view state's scene parent. */
	void SetViewParent(FSceneViewStateInterface* InViewParent)
	{
		if ( ViewParent )
		{
			// Assert that the existing parent does not have a parent.
			check( !ViewParent->HasViewParent() );
			// Decrement ref ctr of existing parent.
			--ViewParent->NumChildren;
		}

		if ( InViewParent && InViewParent != this )
		{
			// Assert that the incoming parent does not have a parent.
			check( !InViewParent->HasViewParent() );
			ViewParent = InViewParent;
			// Increment ref ctr of new parent.
			InViewParent->NumChildren++;
		}
		else
		{
			ViewParent = NULL;
		}
	}
	/** @return			The view state's scene parent, or NULL if none present. */
	FSceneViewStateInterface* GetViewParent()
	{
		return ViewParent;
	}
	/** @return			The view state's scene parent, or NULL if none present. */
	const FSceneViewStateInterface* GetViewParent() const
	{
		return ViewParent;
	}
	/** @return			TRUE if the scene state has a parent, FALSE otherwise. */
	UBOOL HasViewParent() const
	{
		return GetViewParent() != NULL;
	}
	/** @return			TRUE if this scene state is a parent, FALSE otherwise. */
	UBOOL IsViewParent() const
	{
		return NumChildren > 0;
	}

	virtual SIZE_T GetSizeBytes() const { return 0; }

protected:
	// Don't allow direct deletion of the view state, Destroy should be called instead.
	virtual ~FSceneViewStateInterface() {}

private:
	/** This scene state's view parent; NULL if no parent present. */
	FSceneViewStateInterface*	ViewParent;
	/** Reference counts the number of children parented to this state. */
	INT							NumChildren;
};

/**
 * Allocates a new instance of the private scene manager implementation of FSceneViewStateInterface
 */
extern FSceneViewStateInterface* AllocateViewState();

/**
 * The types of interactions between a light and a primitive.
 */
enum ELightInteractionType
{
	LIT_CachedIrrelevant,
	LIT_CachedLightMap,
	LIT_CachedShadowMap1D,
	LIT_CachedShadowMap2D,
	LIT_CachedSignedDistanceFieldShadowMap2D,
	LIT_Uncached
};

/**
 * Information about an interaction between a light and a mesh.
 */
class FLightInteraction
{
public:

	// Factory functions.
	static FLightInteraction Uncached() { return FLightInteraction(LIT_Uncached,NULL,FVector2D(EC_EventParm),FVector2D(EC_EventParm)); }
	static FLightInteraction LightMap() { return FLightInteraction(LIT_CachedLightMap,NULL,FVector2D(EC_EventParm),FVector2D(EC_EventParm)); }
	static FLightInteraction Irrelevant() { return FLightInteraction(LIT_CachedIrrelevant,NULL,FVector2D(EC_EventParm),FVector2D(EC_EventParm)); }
	static FLightInteraction ShadowMap1D(const FVertexBuffer* ShadowVertexBuffer)
	{
		return FLightInteraction(LIT_CachedShadowMap1D,ShadowVertexBuffer,FVector2D(EC_EventParm),FVector2D(EC_EventParm));
	}
	static FLightInteraction ShadowMap2D(
		const UTexture2D* ShadowTexture,
		const FVector2D& InShadowCoordinateScale,
		const FVector2D& InShadowCoordinateBias,
		UBOOL bIsShadowFactorTexture
		)
	{
		return FLightInteraction(bIsShadowFactorTexture ? LIT_CachedShadowMap2D : LIT_CachedSignedDistanceFieldShadowMap2D,ShadowTexture,InShadowCoordinateScale,InShadowCoordinateBias);
	}

	// Accessors.
	ELightInteractionType GetType() const { return Type; }
	const FVertexBuffer* GetShadowVertexBuffer() const
	{
		check(Type == LIT_CachedShadowMap1D);
		return (FVertexBuffer*)ShadowResource;
	}
	const UTexture2D* GetShadowTexture() const
	{
		check(Type == LIT_CachedShadowMap2D || Type == LIT_CachedSignedDistanceFieldShadowMap2D);
		return (UTexture2D*)ShadowResource;
	}
	const FVector2D& GetShadowCoordinateScale() const
	{
		check(Type == LIT_CachedShadowMap2D || Type == LIT_CachedSignedDistanceFieldShadowMap2D);
		return ShadowCoordinateScale;
	}
	const FVector2D& GetShadowCoordinateBias() const
	{
		check(Type == LIT_CachedShadowMap2D || Type == LIT_CachedSignedDistanceFieldShadowMap2D);
		return ShadowCoordinateBias;
	}

private:

	/**
	 * Minimal initialization constructor.
	 */
	FLightInteraction(
		ELightInteractionType InType,
		const void* InShadowResource,
		const FVector2D& InShadowCoordinateScale,
		const FVector2D& InShadowCoordinateBias
		):
		Type(InType),
		ShadowResource(InShadowResource),
		ShadowCoordinateScale(InShadowCoordinateScale),
		ShadowCoordinateBias(InShadowCoordinateBias)
	{}

	ELightInteractionType Type;
	const void* ShadowResource;
	FVector2D ShadowCoordinateScale;
	FVector2D ShadowCoordinateBias;
};

/**
 * The types of interactions between a light and a primitive.
 */
enum ELightMapInteractionType
{
	LMIT_None,
	LMIT_Vertex,
	LMIT_Texture,
};

/** The number of coefficients that are stored for each light sample. */ 
static const INT NUM_STORED_LIGHTMAP_COEF = 3;

/** The number of directional coefficients which the lightmap stores for each light sample. */ 
static const INT NUM_DIRECTIONAL_LIGHTMAP_COEF = 2;

/** The number of simple coefficients which the lightmap stores for each light sample. */ 
static const INT NUM_SIMPLE_LIGHTMAP_COEF = 1;

/** The index at which simple coefficients are stored in any array containing all NUM_STORED_LIGHTMAP_COEF coefficients. */ 
static const INT SIMPLE_LIGHTMAP_COEF_INDEX = 2;

/** Compile out simple lightmaps to save memory */
#define ALLOW_SIMPLE_LIGHTMAPS (!CONSOLE || (MOBILE))

/** Compile out directional lightmaps to save memory */
#define ALLOW_DIRECTIONAL_LIGHTMAPS (!MOBILE || FLASH)

/** Make sure at least one is defined */
#if !ALLOW_SIMPLE_LIGHTMAPS && !ALLOW_DIRECTIONAL_LIGHTMAPS
#error At least one of ALLOW_SIMPLE_LIGHTMAPS and ALLOW_DIRECTIONAL_LIGHTMAPS needs to be defined!
#endif

/**
 * Information about an interaction between a light and a mesh.
 */
class FLightMapInteraction
{
public:

	// Factory functions.
	static FLightMapInteraction None()
	{
		FLightMapInteraction Result;
		Result.Type = LMIT_None;
		return Result;
	}
	static FLightMapInteraction Texture(
		const class ULightMapTexture2D* const* InTextures,
		const FVector4* InCoefficientScales,
		const FVector2D& InCoordinateScale,
		const FVector2D& InCoordinateBias,
		UBOOL bAllowDirectionalLightMaps)
	{
		FLightMapInteraction Result;
		Result.Type = LMIT_Texture;

		UBOOL bUseDirectionalLightMaps = GSystemSettings.bAllowDirectionalLightMaps;

#if ALLOW_SIMPLE_LIGHTMAPS && ALLOW_DIRECTIONAL_LIGHTMAPS
		// however, if simple and directional are allowed, then we must use the value passed in,
		// and then cache the number as well
		bUseDirectionalLightMaps = bUseDirectionalLightMaps && bAllowDirectionalLightMaps;

		Result.bAllowDirectionalLightMaps = bUseDirectionalLightMaps;
		if (bAllowDirectionalLightMaps)
		{
			Result.NumLightmapCoefficients = NUM_DIRECTIONAL_LIGHTMAP_COEF;
		}
		else
		{
			Result.NumLightmapCoefficients = NUM_SIMPLE_LIGHTMAP_COEF;
		}
#endif

		//copy over the appropriate textures and scales
		if (bUseDirectionalLightMaps)
		{
#if ALLOW_DIRECTIONAL_LIGHTMAPS
			for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF;CoefficientIndex++)
			{
				Result.DirectionalTextures[CoefficientIndex] = InTextures[CoefficientIndex];
				Result.DirectionalCoefficientScales[CoefficientIndex] = InCoefficientScales[CoefficientIndex];
			}
#endif
		}

		// NOTE: In PC editor we cache both Simple and Directional textures as we may need to dynamically switch between them
		if( GIsEditor || !bUseDirectionalLightMaps )
		{
#if ALLOW_SIMPLE_LIGHTMAPS
			Result.SimpleTextures[0] = InTextures[SIMPLE_LIGHTMAP_COEF_INDEX];
			Result.SimpleCoefficientScales[0] = InCoefficientScales[SIMPLE_LIGHTMAP_COEF_INDEX];
#endif
		}

		Result.CoordinateScale = InCoordinateScale;
		Result.CoordinateBias = InCoordinateBias;
		return Result;
	}
	static FLightMapInteraction Vertex(const FVertexBuffer* InVertexBuffer,const FVector4* InCoefficientScales,UBOOL bAllowDirectionalLightMaps)
	{
		FLightMapInteraction Result;
		Result.Type = LMIT_Vertex;
		Result.VertexBuffer = InVertexBuffer;

		UBOOL bUseDirectionalLightMaps = GSystemSettings.bAllowDirectionalLightMaps;

#if ALLOW_SIMPLE_LIGHTMAPS && ALLOW_DIRECTIONAL_LIGHTMAPS
		// however, if simple and directional are allowed, then we must use the value passed in,
		// and then cache the number as well
		bUseDirectionalLightMaps = bUseDirectionalLightMaps && bAllowDirectionalLightMaps;

		Result.bAllowDirectionalLightMaps = bAllowDirectionalLightMaps;
		if (bAllowDirectionalLightMaps)
		{
			Result.NumLightmapCoefficients = NUM_DIRECTIONAL_LIGHTMAP_COEF;
		}
		else
		{
			Result.NumLightmapCoefficients = NUM_SIMPLE_LIGHTMAP_COEF;
		}
#endif

		//copy over the appropriate textures and scales
		if (bUseDirectionalLightMaps)
		{
#if ALLOW_DIRECTIONAL_LIGHTMAPS
			for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF;CoefficientIndex++)
			{
				Result.DirectionalCoefficientScales[CoefficientIndex] = InCoefficientScales[CoefficientIndex];
			}
#endif
		}

		// NOTE: In PC editor we cache both Simple and Directional textures as we may need to dynamically switch between them
		if( GIsEditor || !bUseDirectionalLightMaps )
		{
#if ALLOW_SIMPLE_LIGHTMAPS
			Result.SimpleCoefficientScales[0] = InCoefficientScales[SIMPLE_LIGHTMAP_COEF_INDEX];
#endif
		}

		return Result;
	}

	/** Default constructor. */
	FLightMapInteraction():
		Type(LMIT_None)
	{}

	// Accessors.
	ELightMapInteractionType GetType() const { return Type; }
	const FVertexBuffer* GetVertexBuffer() const
	{
		check(Type == LMIT_Vertex);
		return VertexBuffer;
	}
	
	const ULightMapTexture2D* GetTexture(INT TextureIndex) const
	{
		check(Type == LMIT_Texture);
#if ALLOW_SIMPLE_LIGHTMAPS && ALLOW_DIRECTIONAL_LIGHTMAPS
		return AllowsDirectionalLightmaps() ? DirectionalTextures[TextureIndex] : SimpleTextures[TextureIndex];
#elif ALLOW_DIRECTIONAL_LIGHTMAPS
		return DirectionalTextures[TextureIndex];
#else
		return SimpleTextures[TextureIndex];
#endif
	}
	const FVector4* GetScaleArray() const
	{
#if ALLOW_SIMPLE_LIGHTMAPS && ALLOW_DIRECTIONAL_LIGHTMAPS
		return AllowsDirectionalLightmaps() ? DirectionalCoefficientScales : SimpleCoefficientScales;
#elif ALLOW_DIRECTIONAL_LIGHTMAPS
		return DirectionalCoefficientScales;
#else
		return SimpleCoefficientScales;
#endif
	}
	
	const FVector2D& GetCoordinateScale() const
	{
		check(Type == LMIT_Texture);
		return CoordinateScale;
	}
	const FVector2D& GetCoordinateBias() const
	{
		check(Type == LMIT_Texture);
		return CoordinateBias;
	}

	UINT GetNumLightmapCoefficients() const
	{
#if ALLOW_SIMPLE_LIGHTMAPS && ALLOW_DIRECTIONAL_LIGHTMAPS
#if !CONSOLE && !FINAL_RELEASE		// This is to allow for dynamic switching between simple and directional light maps in the PC editor
		if( !AllowsDirectionalLightmaps() )
		{
			return NUM_SIMPLE_LIGHTMAP_COEF;
		}
#endif
		return NumLightmapCoefficients;
#elif ALLOW_DIRECTIONAL_LIGHTMAPS
		return NUM_DIRECTIONAL_LIGHTMAP_COEF;
#else
		return NUM_SIMPLE_LIGHTMAP_COEF;
#endif
	}

	/**
	* @return TRUE if non-simple lightmaps are allowed
	*/
	FORCEINLINE UBOOL AllowsDirectionalLightmaps() const
	{
#if ALLOW_SIMPLE_LIGHTMAPS && ALLOW_DIRECTIONAL_LIGHTMAPS
		return bAllowDirectionalLightMaps
#if !CONSOLE && !FINAL_RELEASE	// This is to allow for dynamic switching between simple and directional light maps in the PC editor
			 && !GUseSimpleLightmapsForMobileEmulation
#endif
			;
#elif ALLOW_DIRECTIONAL_LIGHTMAPS
		return TRUE;
#else
		return FALSE;
#endif
	}

	/** These functions are used for the Dummy lightmap policy used in LightMap density view mode. */
	/** 
	 *	Set the type.
	 *
	 *	@param	InType				The type to set it to.
	 */
	void SetLightMapInteractionType(ELightMapInteractionType InType)
	{
		Type = InType;
	}
	/** 
	 *	Set the coordinate scale.
	 *
	 *	@param	InCoordinateScale	The scale to set it to.
	 */
	void SetCoordinateScale(const FVector2D& InCoordinateScale)
	{
		CoordinateScale = InCoordinateScale;
	}
	/** 
	 *	Set the coordinate bias.
	 *
	 *	@param	InCoordinateBias	The bias to set it to.
	 */
	void SetCoordinateBias(const FVector2D& InCoordinateBias)
	{
		CoordinateBias = InCoordinateBias;
	}

private:

#if ALLOW_DIRECTIONAL_LIGHTMAPS
	FVector4 DirectionalCoefficientScales[NUM_DIRECTIONAL_LIGHTMAP_COEF];
	const class ULightMapTexture2D* DirectionalTextures[NUM_DIRECTIONAL_LIGHTMAP_COEF];
#endif

#if ALLOW_SIMPLE_LIGHTMAPS
	FVector4 SimpleCoefficientScales[NUM_SIMPLE_LIGHTMAP_COEF];
	const class ULightMapTexture2D* SimpleTextures[NUM_SIMPLE_LIGHTMAP_COEF];
#endif

#if ALLOW_SIMPLE_LIGHTMAPS && ALLOW_DIRECTIONAL_LIGHTMAPS
	UBOOL bAllowDirectionalLightMaps;
	UINT NumLightmapCoefficients;
#endif

	ELightMapInteractionType Type;
	const FVertexBuffer* VertexBuffer;

	FVector2D CoordinateScale;
	FVector2D CoordinateBias;
};

/**
 * An interface to cached lighting for a specific mesh.
 */
class FLightCacheInterface
{
public:
	virtual FLightInteraction GetInteraction(const class FLightSceneInfo* LightSceneInfo) const = 0;
	virtual FLightMapInteraction GetLightMapInteraction() const = 0;
};

/**
 * Motion blur info used for mesh elements
 */
struct FMeshElementMotionBlurInfo
{
	/** The previous LocalToWorld used to render the mesh element.	*/
	FMatrix PreviousLocalToWorld;
};

/**
 * A batch mesh element definition.
 */
struct FMeshBatchElement
{
	FMatrix LocalToWorld;
	FMatrix WorldToLocal;

	const FIndexBuffer* IndexBuffer;
	UINT FirstIndex;
	UINT NumPrimitives;
	UINT DegenerateTriangleCount;
	UINT MinVertexIndex;
	UINT MaxVertexIndex;
	void* ElementUserData;

	/** 
	 *	DynamicIndexData - pointer to user memory containing the index data.
	 *	Used for rendering dynamic data directly.
	 */
	const void* DynamicIndexData;
	WORD DynamicIndexStride;
	
	FMeshBatchElement()
	:	IndexBuffer(NULL)
	,	NumPrimitives(0)
	,	DegenerateTriangleCount(0)
	,	ElementUserData(NULL)
	,	DynamicIndexData(NULL)
	{
	}
};

/**
 * A batch of mesh elements, all with the same material and vertex buffer
 */
struct FMeshBatch
{
	TArray<FMeshBatchElement,TInlineAllocator<1> > Elements;
	FLOAT DepthBias;
	FLOAT SlopeScaleDepthBias;
	BITFIELD UseDynamicData : 1;
	BITFIELD ReverseCulling : 1;
	BITFIELD bDisableBackfaceCulling : 1;
	BITFIELD CastShadow : 1;
	BITFIELD bWireframe : 1;
	BITFIELD Type : PT_NumBits;
	BITFIELD ParticleType : PET_NumBits;
	BITFIELD DepthPriorityGroup : UCONST_SDPG_NumBits;
	BITFIELD bUseAsOccluder : 1;
	BITFIELD bIsDecal : 1;
	BITFIELD bUseDownsampledTranslucency : 1;
	BITFIELD SpriteScreenAlignment : 2;
	BITFIELD bUsePreVertexShaderCulling : 1;
	BITFIELD BitfieldPadding : 12; // Explicitly pad the bitfield to 4 bytes.

	/**	DecalState - pointer to a dynamically generated decal that wants to use a scissor rect when rendering. */
	const class FDecalState* DecalState;

	const FLightCacheInterface* LCI;

	/** 
	 *	DynamicVertexData - pointer to user memory containing the vertex data.
	 *	Used for rendering dynamic data directly.
	 */
	const void* DynamicVertexData;
	WORD DynamicVertexStride;

	/** Optional motion blur info for the mesh element */
	const FMeshElementMotionBlurInfo* MBInfo;

	const FVertexFactory* VertexFactory;
	const FMaterialRenderProxy* MaterialRenderProxy;

	void *PlatformMeshData; // FPlatformStaticMeshData

	/** LOD index of the mesh, used for fading LOD transitions. */
	SBYTE LODIndex;

	/** Optional data for instancing LOD */
	const struct FInstancingLODData* InstancingLODData;

	FORCEINLINE UBOOL IsTranslucent() const
	{
        return MaterialRenderProxy && IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial()->GetBlendMode());
	}

	FORCEINLINE UBOOL IsSoftMasked() const
	{
		return MaterialRenderProxy && MaterialRenderProxy->GetMaterial()->GetBlendMode() == BLEND_SoftMasked;
	}

	FORCEINLINE UBOOL IsMasked() const
	{
		return MaterialRenderProxy && MaterialRenderProxy->GetMaterial()->IsMasked();
	}

	FORCEINLINE UBOOL IsDistortion() const
	{
		return MaterialRenderProxy && MaterialRenderProxy->GetMaterial()->IsDistorted();
	}

	FORCEINLINE UBOOL HasSubsurfaceScattering() const
	{
		return MaterialRenderProxy && MaterialRenderProxy->GetMaterial()->HasSubsurfaceScattering();
	}

	FORCEINLINE UBOOL HasSeparateTranslucency() const
	{
		return MaterialRenderProxy && MaterialRenderProxy->GetMaterial()->HasSeparateTranslucency();
	}

	FORCEINLINE UBOOL RenderLitTranslucencyPrepass() const
	{
		return MaterialRenderProxy && MaterialRenderProxy->GetMaterial()->RenderLitTranslucencyPrepass();
	}

	FORCEINLINE UBOOL RenderLitTranslucencyDepthPostpass() const
	{
		return MaterialRenderProxy && MaterialRenderProxy->GetMaterial()->RenderLitTranslucencyDepthPostpass();
	}
	
	//@return TRUE if the mesh element is used for decal rendering
	FORCEINLINE UBOOL IsDecal() const
	{
		return bIsDecal;
	}

	/** Converts from an INT index into a SBYTE */
	static SBYTE QuantizeLODIndex(INT NewLODIndex)
	{
		checkSlow(NewLODIndex >= SCHAR_MIN && NewLODIndex <= SCHAR_MAX);
		return (SBYTE)NewLODIndex;
	}

	/** 
	* @return vertex stride specified for the mesh. 0 if not dynamic
	*/
	FORCEINLINE DWORD GetDynamicVertexStride() const
	{
		if (UseDynamicData && DynamicVertexData)
		{
			return DynamicVertexStride;
		}
		else
		{
			return 0;
		}
	}

	FORCEINLINE INT GetNumPrimitives() const
	{
		INT Count=0;
		for( INT ElementIdx=0;ElementIdx<Elements.Num();ElementIdx++ )
		{
			Count += Elements(ElementIdx).NumPrimitives;
		}
		return Count;
	}

	/** Default constructor. */
	FMeshBatch() :
		DepthBias(0.f),
		SlopeScaleDepthBias(0.f),
		UseDynamicData(FALSE),
		ReverseCulling(FALSE),
		bDisableBackfaceCulling(FALSE),
		CastShadow(TRUE),
		bWireframe(FALSE),
		Type(PT_TriangleList),
		ParticleType(PET_None),
		bUseAsOccluder(TRUE),
		bIsDecal(FALSE),
		bUseDownsampledTranslucency(FALSE),
		SpriteScreenAlignment(SSA_CameraFacing),
		bUsePreVertexShaderCulling(FALSE),
		DecalState(NULL),
		LCI(NULL),
		DynamicVertexData(NULL),
		MBInfo(NULL),
		VertexFactory(NULL),
		MaterialRenderProxy(NULL),
		PlatformMeshData(NULL),
		LODIndex(INDEX_NONE),
		InstancingLODData(NULL)
		{
			// By default always add the first element.
			new(Elements) FMeshBatchElement;
		}
};

/**
 * An interface implemented by dynamic resources which need to be initialized and cleaned up by the rendering thread.
 */
class FDynamicPrimitiveResource
{
public:

	virtual void InitPrimitiveResource() = 0;
	virtual void ReleasePrimitiveResource() = 0;
};

/**
 * The base interface used to query a primitive for its dynamic elements.
 */
class FPrimitiveDrawInterface
{
public:

	const FSceneView* const View;

	/** Initialization constructor. */
	FPrimitiveDrawInterface(const FSceneView* InView):
		View(InView)
	{}

	virtual UBOOL IsHitTesting() = 0;
	virtual void SetHitProxy(HHitProxy* HitProxy) = 0;

	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource) = 0;

	virtual void DrawSprite(
		const FVector& Position,
		FLOAT SizeX,
		FLOAT SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		BYTE DepthPriorityGroup,
		FLOAT U,
		FLOAT UL,
		FLOAT V,
		FLOAT VL,
		BYTE BlendMode = 1 /*SE_BLEND_Masked*/
		) = 0;

	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		BYTE DepthPriorityGroup,
		const FLOAT Thickness = 0.0f
		) = 0;

	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		FLOAT PointSize,
		BYTE DepthPriorityGroup
		) = 0;

	/**
	 * Determines whether a particular material will be ignored in this context.
	 * @param MaterialRenderProxy - The render proxy of the material to check.
	 * @return TRUE if meshes using the material will be ignored in this context.
	 */
	virtual UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy) const
	{
		return FALSE;
	}

	/**
	 * Draw a mesh element.
	 * This should only be called through the DrawMesh function.
	 *
	 * @return Number of passes rendered for the mesh
	 */
	virtual INT DrawMesh(const FMeshBatch& Mesh) = 0;

	/**
	 * @return TRUE if rendering is occuring during velocity pass 
	 */
	virtual UBOOL IsRenderingVelocities() const { return FALSE; }
};

/**
 * An interface to a scene interaction.
 */
class FViewElementDrawer
{
public:

	/**
	 * Draws the interaction using the given draw interface.
	 */
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) {}
};

/**
 * An interface used to query a primitive for its static elements.
 */
class FStaticPrimitiveDrawInterface
{
public:
	virtual void SetHitProxy(HHitProxy* HitProxy) = 0;
	virtual void DrawMesh(
		const FMeshBatch& Mesh,
		FLOAT MinDrawDistance,
		FLOAT MaxDrawDistance
		) = 0;
};

/**
 * The different types of relevance a primitive scene proxy can declare towards a particular scene view.
 */
struct FPrimitiveViewRelevance
{
	/** The primitive's static elements are rendered for the view. */
	BITFIELD bStaticRelevance : 1; 
	/** The primitive's dynamic elements are rendered for the view. */
	BITFIELD bDynamicRelevance : 1;
	/** If TRUE, the primitive would normally have bStaticRelevance but static relevance is disabled because it is currently fading. */
	BITFIELD bStaticButFadingRelevance : 1;
	/** The primitive is casting a shadow. */
	BITFIELD bShadowRelevance : 1;
	/** The primitive's static decal elements are rendered for the view. */
	BITFIELD bDecalStaticRelevance : 1;
	/** The primitive's dynamic decal elements are rendered for the view. */
	BITFIELD bDecalDynamicRelevance : 1;	

	/** The primitive has one or more UnrealEd background DPG elements. */
	BITFIELD bUnrealEdBackgroundDPG : 1;
	/** The primitive has one or more world DPG elements. */
	BITFIELD bWorldDPG : 1;
	/** The primitive has one or more foreground DPG elements. */
	BITFIELD bForegroundDPG : 1;
	/** The primitive has one or more UnrealEd DPG elements. */
	BITFIELD bUnrealEdForegroundDPG : 1;

	/** The primitive has one or more opaque or masked elements. */
	BITFIELD bOpaqueRelevance : 1;
	/** The primitive has one or more masked elements. */
	BITFIELD bMaskedRelevance : 1;
	/** The primitive has one or more translucent elements. */
	BITFIELD bTranslucentRelevance : 1;
	/** Should we render a depth prepass for dynamic lit translucent elements */
	BITFIELD bDynamicLitTranslucencyPrepass: 1;
	/** Should we render a post-translucency pass (for Opacity>0.0 pixels) */
	BITFIELD bDynamicLitTranslucencyPostRenderDepthPass : 1;
	/** The primitive has one or more distortion elements. */
	BITFIELD bDistortionRelevance : 1;
	/** The primitive has one or more one-layer distortion elements. */
	BITFIELD bOneLayerDistortionRelevance : 1;
	/** The primitive has one or more elements with the material setting bTranslucencyInheritDominantShadowsFromOpaque enabled. */
	BITFIELD bInheritDominantShadowsRelevance : 1;
	/** The primitive has one or more lit elements. */
	BITFIELD bLightingRelevance : 1;
	/** The primitive has one or more elements which use a material that samples scene color */
	BITFIELD bUsesSceneColor : 1;
	/** The primitive has one or more elements which use a material that has bSceneTextureRenderBehindTranslucency enabled. */
	BITFIELD bSceneTextureRenderBehindTranslucency : 1;
	/** Forces all directional lights to be dynamic. */
	BITFIELD bForceDirectionalLightsDynamic : 1;

	/** TRUE if the primitive needs PreRenderView to be called before rendering. */
	BITFIELD bNeedsPreRenderView : 1;
	/** The primitive has one or more soft masked elements. */
	BITFIELD bSoftMaskedRelevance : 1;
	/** The translucent elements should render DoF blur factor to the DoF blur buffer */
	BITFIELD bTranslucencyDoFRelevance : 1;
	/** The primitive has one or more elements that have SeparateTranslucency. */
	BITFIELD bSeparateTranslucencyRelevance : 1;
	/** 
	 * Whether this primitive view relevance has been initialized this frame.  
	 * Primitives that have not had FSceneRenderer::ProcessVisible called on them (because they were culled) will not be initialized,
	 * But we may still need to render them from other views like shadow passes, so this tracks whether we can reuse the cached relevance or not.
	 */
	BITFIELD bInitializedThisFrame : 1;


	/** Initialization constructor. */
	FPrimitiveViewRelevance():
		bStaticRelevance(FALSE),
		bDynamicRelevance(FALSE),
		bStaticButFadingRelevance(FALSE),
		bShadowRelevance(FALSE),
		bDecalStaticRelevance(FALSE),
		bDecalDynamicRelevance(FALSE),
		bUnrealEdBackgroundDPG(FALSE),
		bWorldDPG(FALSE),
		bForegroundDPG(FALSE),
		bUnrealEdForegroundDPG(FALSE),
		bOpaqueRelevance(TRUE),
		bMaskedRelevance(FALSE),
		bTranslucentRelevance(FALSE),
		bDynamicLitTranslucencyPrepass(FALSE),
		bDynamicLitTranslucencyPostRenderDepthPass(FALSE),
		bDistortionRelevance(FALSE),
		bOneLayerDistortionRelevance(FALSE),
		bInheritDominantShadowsRelevance(FALSE),
		bLightingRelevance(FALSE),
		bUsesSceneColor(FALSE),
		bSceneTextureRenderBehindTranslucency(FALSE),
		bForceDirectionalLightsDynamic(FALSE),
		bNeedsPreRenderView(FALSE),
		bSoftMaskedRelevance(FALSE),
		bTranslucencyDoFRelevance(FALSE),
		bSeparateTranslucencyRelevance(FALSE),
		bInitializedThisFrame(FALSE)
	{}

	/** Bitwise OR operator.  Sets any relevance bits which are present in either FPrimitiveViewRelevance. */
	FPrimitiveViewRelevance& operator|=(const FPrimitiveViewRelevance& B)
	{
		bStaticRelevance |= B.bStaticRelevance != 0;
		bDynamicRelevance |= B.bDynamicRelevance != 0;
		bShadowRelevance |= B.bShadowRelevance != 0;
		bDecalStaticRelevance |= B.bDecalStaticRelevance != 0;
		bDecalDynamicRelevance |= B.bDecalDynamicRelevance != 0;
		bUnrealEdBackgroundDPG |= B.bUnrealEdBackgroundDPG != 0;
		bWorldDPG |= B.bWorldDPG != 0;
		bForegroundDPG |= B.bForegroundDPG != 0;
		bUnrealEdForegroundDPG |= B.bUnrealEdForegroundDPG != 0;
		bOpaqueRelevance |= B.bOpaqueRelevance != 0;
		bMaskedRelevance |= B.bMaskedRelevance != 0;
		bTranslucentRelevance |= B.bTranslucentRelevance != 0;
		bDynamicLitTranslucencyPrepass |= B.bDynamicLitTranslucencyPrepass != 0;
		bDynamicLitTranslucencyPostRenderDepthPass |= B.bDynamicLitTranslucencyPostRenderDepthPass != 0;
		bDistortionRelevance |= B.bDistortionRelevance != 0;
		bOneLayerDistortionRelevance |= B.bOneLayerDistortionRelevance != 0;
		bInheritDominantShadowsRelevance |= B.bInheritDominantShadowsRelevance != 0;
		bLightingRelevance |= B.bLightingRelevance != 0;
		bUsesSceneColor |= B.bUsesSceneColor != 0;
		bSceneTextureRenderBehindTranslucency |= B.bSceneTextureRenderBehindTranslucency != 0;
		bForceDirectionalLightsDynamic |= B.bForceDirectionalLightsDynamic != 0;
		bNeedsPreRenderView |= B.bNeedsPreRenderView != 0;
		bSoftMaskedRelevance |= B.bSoftMaskedRelevance != 0;
		bTranslucencyDoFRelevance |= B.bTranslucencyDoFRelevance != 0;
		bSeparateTranslucencyRelevance |= B.bSeparateTranslucencyRelevance != 0;
		bInitializedThisFrame |= B.bInitializedThisFrame;
		return *this;
	}

	/** Binary bitwise OR operator. */
	friend FPrimitiveViewRelevance operator|(const FPrimitiveViewRelevance& A,const FPrimitiveViewRelevance& B)
	{
		FPrimitiveViewRelevance Result(A);
		Result |= B;
		return Result;
	}

	/** Sets the flag corresponding to the given DPG. */
	void SetDPG(UINT DPGIndex,UBOOL bValue)
	{
		switch(DPGIndex)
		{
		case SDPG_UnrealEdBackground:	bUnrealEdBackgroundDPG = bValue; break;
		case SDPG_World:				bWorldDPG = bValue; break;
		case SDPG_Foreground:			bForegroundDPG = bValue; break;
		case SDPG_UnrealEdForeground:	bUnrealEdForegroundDPG = bValue; break;
		}
	}

	/** Gets the flag corresponding to the given DPG. */
	UBOOL GetDPG(UINT DPGIndex) const
	{
		switch(DPGIndex)
		{
		case SDPG_UnrealEdBackground:	return bUnrealEdBackgroundDPG;
		case SDPG_World:				return bWorldDPG;
		case SDPG_Foreground:			return bForegroundDPG;
		case SDPG_UnrealEdForeground:	return bUnrealEdForegroundDPG;
		default:						return FALSE;
		}
	}

	/** @return True if the primitive is relevant to the view. */
	UBOOL IsRelevant() const
	{
		return (bStaticRelevance | bDynamicRelevance) != 0;
	}

	/** @return True if the primitive has decals which are relevant to the view. */
	UBOOL IsDecalRelevant() const
	{
		return (bDecalStaticRelevance | bDecalDynamicRelevance) != 0;
	}
};

/**
 * View relevance information about a material or set of materials.
 * This is mirrored in PrimitiveComponent.uc
 */
class FMaterialViewRelevance
{
public:

	BITFIELD bOpaque : 1;
	BITFIELD bMasked : 1;
	BITFIELD bTranslucency : 1;
	BITFIELD bDistortion : 1;
	BITFIELD bOneLayerDistortionRelevance : 1;
	BITFIELD bInheritDominantShadowsRelevance : 1;
	BITFIELD bLit : 1;
	BITFIELD bUsesSceneColor : 1;
	BITFIELD bSceneTextureRenderBehindTranslucency : 1;
	BITFIELD bDynamicLitTranslucencyPrepass : 1;
	BITFIELD bDynamicLitTranslucencyPostRenderDepthPass : 1;
	BITFIELD bSoftMasked : 1;
	BITFIELD bTranslucencyDoF : 1;
	BITFIELD bSeparateTranslucency : 1;

	/** Default constructor. */
	FMaterialViewRelevance():
		bOpaque(FALSE),
		bMasked(FALSE),
		bTranslucency(FALSE),
		bDistortion(FALSE),
		bOneLayerDistortionRelevance(FALSE),
		bInheritDominantShadowsRelevance(FALSE),
		bLit(FALSE),
		bUsesSceneColor(FALSE),
		bSceneTextureRenderBehindTranslucency(FALSE),
		bDynamicLitTranslucencyPrepass(FALSE),
		bDynamicLitTranslucencyPostRenderDepthPass(FALSE),
		bSoftMasked(FALSE),
		bTranslucencyDoF(FALSE),
		bSeparateTranslucency(FALSE)
	{}

	/** Bitwise OR operator.  Sets any relevance bits which are present in either FMaterialViewRelevance. */
	FMaterialViewRelevance& operator|=(const FMaterialViewRelevance& B)
	{
		bOpaque |= B.bOpaque;
		bMasked |= B.bMasked;
		bTranslucency |= B.bTranslucency;
		bDistortion |= B.bDistortion;
		bOneLayerDistortionRelevance |= B.bOneLayerDistortionRelevance;
		bInheritDominantShadowsRelevance |= B.bInheritDominantShadowsRelevance;
		bLit |= B.bLit;
		bUsesSceneColor |= B.bUsesSceneColor;
		bSceneTextureRenderBehindTranslucency |= B.bSceneTextureRenderBehindTranslucency;
		bDynamicLitTranslucencyPrepass |= B.bDynamicLitTranslucencyPrepass;
		bDynamicLitTranslucencyPostRenderDepthPass |= B.bDynamicLitTranslucencyPostRenderDepthPass;
		bSoftMasked |= B.bSoftMasked;
		bTranslucencyDoF |= B.bTranslucencyDoF;
		bSeparateTranslucency |= B.bSeparateTranslucency;
		return *this;
	}
	
	/** Binary bitwise OR operator. */
	friend FMaterialViewRelevance operator|(const FMaterialViewRelevance& A,const FMaterialViewRelevance& B)
	{
		FMaterialViewRelevance Result(A);
		Result |= B;
		return Result;
	}

	/** Copies the material's relevance flags to a primitive's view relevance flags. */
	void SetPrimitiveViewRelevance(FPrimitiveViewRelevance& OutViewRelevance) const
	{
		OutViewRelevance.bOpaqueRelevance = bOpaque;
		OutViewRelevance.bMaskedRelevance = bMasked;
		OutViewRelevance.bTranslucentRelevance = bTranslucency;
		OutViewRelevance.bDistortionRelevance = bDistortion;
		OutViewRelevance.bOneLayerDistortionRelevance = bOneLayerDistortionRelevance;
		OutViewRelevance.bInheritDominantShadowsRelevance = bInheritDominantShadowsRelevance;
		OutViewRelevance.bLightingRelevance = bLit;
		OutViewRelevance.bUsesSceneColor = bUsesSceneColor;
		OutViewRelevance.bSceneTextureRenderBehindTranslucency = bSceneTextureRenderBehindTranslucency;
		OutViewRelevance.bDynamicLitTranslucencyPrepass = bDynamicLitTranslucencyPrepass;
		OutViewRelevance.bDynamicLitTranslucencyPostRenderDepthPass = bDynamicLitTranslucencyPostRenderDepthPass;
		OutViewRelevance.bSoftMaskedRelevance = bSoftMasked;
		OutViewRelevance.bTranslucencyDoFRelevance = bTranslucencyDoF;
		OutViewRelevance.bSeparateTranslucencyRelevance = bSeparateTranslucency;
	}
};

/**
 *	Helper structure for storing motion blur information for a primitive
 */
struct FMotionBlurInfo
{
	FMotionBlurInfo()
	{
		Invalidate();
		bDeleteMe = FALSE;
	}

	void SetMotionBlurInfo(UPrimitiveComponent* InComponent, FPrimitiveSceneInfo* InPrimitiveSceneInfo, const FMatrix& InPreviousLocalToWorld, UINT FrameNumber)
	{
		if(bUpdated && PrimitiveUpdateFrameNumber == FrameNumber)
		{
			// prevent double update
			return;
		}

		PrimitiveUpdateFrameNumber = FrameNumber;
		Component = InComponent;
		PrimitiveSceneInfo = InPrimitiveSceneInfo;
		PreviousLocalToWorld = InPreviousLocalToWorld;

		bUpdated = TRUE;
		bDeleteMe = FALSE;
	}

	void Invalidate()
	{
		Component = NULL;
		PrimitiveSceneInfo = NULL;
		bUpdated = FALSE;
		bDeleteMe = FALSE;
	}

	void MarkNotUpdated()
	{
		bUpdated = FALSE;
	}

	UBOOL IsUpdated() const
	{
		return bUpdated; 
	}

	void MarkForDelete()
	{
		bDeleteMe = TRUE;
	}

	UBOOL IsMarkForDelete() const
	{
		return bDeleteMe; 
	}

	/** @return success */
	UBOOL Get(UPrimitiveComponent& InComponent, FMatrix& OutPreviousLocalToWorld) const
	{
		if(&InComponent == Component)
		{
			OutPreviousLocalToWorld = PreviousLocalToWorld;
			return TRUE;
		}
		return FALSE;
	}

	UPrimitiveComponent* GetComponent() const
	{
		return Component;
	}

	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const
	{
		return PrimitiveSceneInfo;
	}

private:
	/** The component this info represents.			*/
	UPrimitiveComponent* Component;
	/** The primitive scene info for the component.	*/
	FPrimitiveSceneInfo* PrimitiveSceneInfo;
	/** The previous LocalToWorld of the component.	*/
	FMatrix	PreviousLocalToWorld;
	/** if TRUE then the PreviousLocalToWorld has already been updated for the current frame */
	UBOOL bUpdated;
	/** Mark for deletion at the end of the frame */
	UBOOL bDeleteMe;
	/** To prevent double updates and ending up with the wrong matrix */
	UINT PrimitiveUpdateFrameNumber;
};

/** Class which allocates channels of a texture for lights based on priority. */
class FLightChannelAllocator
{
public:

	FLightChannelAllocator() :
		NumChannels(INDEX_NONE),
		DomDirectionalLight(INDEX_NONE, 0)
	{}

	/** Resets all allocations and sets a new NumChannels. */
	void Reset(INT InNumChannels);

	/** Adds a new light allocation. */
	void AllocateLight(INT LightId, FLOAT Priority, UBOOL bDomDirectionalLight);

	/** Returns the channel index that the given light should use, or INDEX_NONE if the light was not allocated. */
	INT GetLightChannel(INT LightId) const;

	/** Returns the index of the texture that the light has been allocated in, either 0 or 1. */
	INT GetTextureIndex(INT LightId) const;

	/** Returns the index of the texture that the dominant directional light has been allocated to, or INDEX_NONE if it was never allocated. */
	INT GetDominantDirectionalLightTexture() const;

	INT GetNumChannels() const { return NumChannels; }

private:
	INT NumChannels;

	class FLightChannelInfo
	{
	public:
		INT LightId;
		FLOAT Priority;

		FLightChannelInfo(INT InLightId, FLOAT InPriority) : 
			LightId(InLightId),
			Priority(InPriority)
		{}
	};

	TArray<FLightChannelInfo> Lights;
	FLightChannelInfo DomDirectionalLight;
};

/**
 * An interface to the private scene manager implementation of a scene.  Use AllocateScene to create.
 * The scene
 */
class FSceneInterface
{
public:

	// FSceneInterface interface

	/** 
	 * Adds a new primitive component to the scene
	 * 
	 * @param Primitive - primitive component to add
	 */
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) = 0;
	/** 
	 * Removes a primitive component from the scene
	 * 
	 * @param Primitive - primitive component to remove
	 */
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive, UBOOL bWillReattach) = 0;
	/** 
	 * Updates the transform of a primitive which has already been added to the scene. 
	 * 
	 * @param Primitive - primitive component to update
	 */
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) = 0;
	/** Updates the AffectingDominantLight property of a primitive. */
	virtual void UpdatePrimitiveAffectingDominantLight(UPrimitiveComponent* Primitive, ULightComponent* NewAffectingDominantLight) {}
	/** 
	 * Adds a new light component to the scene
	 * 
	 * @param Light - light component to add
	 */
	virtual void AddLight(ULightComponent* Light) = 0;
	/** 
	 * Removes a light component from the scene
	 * 
	 * @param Light - light component to remove
	 */
	virtual void RemoveLight(ULightComponent* Light) = 0;
	/** 
	 * Updates the transform of a light which has already been added to the scene. 
	 *
	 * @param Light - light component to update
	 */
	virtual void UpdateLightTransform(ULightComponent* Light) = 0;
	/** 
	 * Updates the color and brightness of a light which has already been added to the scene. 
	 *
	 * @param Light - light component to update
	 */
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light) = 0;
	/** Updates the skylight color used on statically lit primitives that are unbuilt. */
	virtual void UpdatePreviewSkyLightColor(const FLinearColor& NewColor) {}
	/** 
	 * Adds a new scene capture component to the scene
	 * 
	 * @param CaptureComponent - scene capture component to add
	 */	
	virtual void AddSceneCapture(USceneCaptureComponent* CaptureComponent) = 0;
	/** 
	 * Removes a scene capture component from the scene
	 * 
	 * @param CaptureComponent - scene capture component to remove
	 */	
	virtual void RemoveSceneCapture(USceneCaptureComponent* CaptureComponent) = 0;
	/**
	 * Adds a fluidsurface to the scene (gamethread)
	 *
	 * @param FluidComponent - component to add to the scene 
	 */
	virtual void AddFluidSurface(class UFluidSurfaceComponent* FluidComponent) = 0;
	/**
	 * Removes a fluidsurface from the scene (gamethread)
	 *
	 * @param CaptureComponent - component to remove from the scene 
	 */
	virtual void RemoveFluidSurface(class UFluidSurfaceComponent* FluidComponent) = 0;

	/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVisibility(const class FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler) {}

	/** Sets the precomputed volume distance field for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVolumeDistanceField(const class FPrecomputedVolumeDistanceField* PrecomputedVolumeDistanceField) {}

	/**
	 * Retrieves a pointer to the fluidsurface container.
	 * @return TArray pointer, or NULL if the scene doesn't support fluidsurfaces.
	 **/
	virtual const TArray<class UFluidSurfaceComponent*>* GetFluidSurfaces() = 0;
	/** Returns a pointer to the first fluid surface detail normal texture in the scene. */
	virtual const FTexture2DRHIRef* GetFluidDetailNormal() const { return NULL; }
	/** 
	 * Adds a new height fog component to the scene
	 * 
	 * @param FogComponent - fog component to add
	 */	
	virtual void AddHeightFog(class UHeightFogComponent* FogComponent) = 0;
	/** 
	 * Removes a height fog component from the scene
	 * 
	 * @param FogComponent - fog component to remove
	 */	
	virtual void RemoveHeightFog(class UHeightFogComponent* FogComponent) = 0;
	/** 
	 * Adds a new exponential height fog component to the scene
	 * 
	 * @param FogComponent - fog component to add
	 */	
	virtual void AddExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) = 0;
	/** 
	 * Removes a exponential height fog component from the scene
	 * 
	 * @param FogComponent - fog component to remove
	 */	
	virtual void RemoveExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) = 0;
	/** 
	 * Adds a new default fog volume scene info to the scene
	 * 
	 * @param VolumeComponent - fog primitive to add
	 */	
	virtual void AddFogVolume(const UPrimitiveComponent* MeshComponent) = 0;
	/** 
	 * Adds a new fog volume scene info to the scene
	 * 
	 * @param FogVolumeSceneInfo - fog volume scene info to add
	 * @param VolumeComponent - fog primitive to add
	 */	
	virtual void AddFogVolume(const class UFogVolumeDensityComponent* FogVolumeComponent, const UPrimitiveComponent* MeshComponent) = 0;
	/** 
	 * Removes a fog volume primitive component from the scene
	 * 
	 * @param VolumeComponent - fog primitive to remove
	 */
	virtual void RemoveFogVolume(const UPrimitiveComponent* MeshComponent) = 0;
	/** Adds an image based reflection component to the scene. */
	virtual void AddImageReflection(const UActorComponent* Component, UTexture2D* InReflectionTexture, FLOAT ReflectionScale, const FLinearColor& InReflectionColor, UBOOL bInTwoSided, UBOOL bEnabled) {}
	/** Updates an image based reflection's transform. */
	virtual void UpdateImageReflection(const UActorComponent* Component, UTexture2D* InReflectionTexture, FLOAT ReflectionScale, const FLinearColor& InReflectionColor, UBOOL bInTwoSided, UBOOL bEnabled) {}
	/** Updates the image reflection texture array. */
	virtual void UpdateImageReflectionTextureArray(UTexture2D* Texture) {}
	/** Removes an image based reflection component from the scene. */
	virtual void RemoveImageReflection(const UActorComponent* Component) {}
	/** Adds an image reflection shadow plane component to the scene. */
	virtual void AddImageReflectionShadowPlane(const UActorComponent* Component, const FPlane& InPlane) {}
	/** Removes an image reflection shadow plane component from the scene. */
	virtual void RemoveImageReflectionShadowPlane(const UActorComponent* Component) {}
	/** Sets scene image reflection environment parameters. */
	virtual void SetImageReflectionEnvironmentTexture(const UTexture2D* NewTexture, const FLinearColor& Color, FLOAT Rotation) {}
	/** Sets the scene in a state where it will not reallocate the image reflection texture array. */
	virtual void BeginPreventIRReallocation() {}
	/** Restores the scene's default state where it will reallocate the image reflection texture array as needed. */
	virtual void EndPreventIRReallocation() {}
	/**
	 * Adds a wind source component to the scene.
	 * @param WindComponent - The component to add.
	 */
	virtual void AddWindSource(class UWindDirectionalSourceComponent* WindComponent) = 0;
	/**
	 * Removes a wind source component from the scene.
	 * @param WindComponent - The component to remove.
	 */
	virtual void RemoveWindSource(class UWindDirectionalSourceComponent* WindComponent) = 0;
	/**
	 * Accesses the wind source list.  Must be called in the rendering thread.
	 * @return The list of wind sources in the scene.
	 */
	virtual const TArray<class FWindSourceSceneProxy*>& GetWindSources_RenderThread() const = 0;

	/** Accesses wind parameters.  XYZ will contain wind direction * Strength, W contains wind speed. */
	virtual FVector4 GetWindParameters(const FVector& Position) const = 0;

	/**
	 * Creates a unique radial blur proxy for the given component and adds it to the scene.
	 *
	 * @param RadialBlurComponent - component being added to the scene
	 */
	virtual void AddRadialBlur(class URadialBlurComponent* RadialBlurComponent) = 0;
	/**
	 * Removes the radial blur proxy for the given component from the scene.
	 *
	 * @param RadialBlurComponent - component being added to the scene
	 */
	virtual void RemoveRadialBlur(class URadialBlurComponent* RadialBlurComponent) = 0;

	/**
	 * Release this scene and remove it from the rendering thread
	 */
	virtual void Release() = 0;
	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const = 0;
	/** 
	 * Indicates if sounds in this should be allowed to play. 
	 * 
	 * @return TRUE if audio playback is allowed
	 */
	virtual UBOOL AllowAudioPlayback() = 0;
	/**
	 * Indicates if hit proxies should be processed by this scene
	 *
	 * @return TRUE if hit proxies should be rendered in this scene.
	 */
	virtual UBOOL RequiresHitProxies() const = 0;
	/**
	 * Get the optional UWorld that is associated with this scene
	 * 
 	 * @return UWorld instance used by this scene
	 */
	virtual class UWorld* GetWorld() const = 0;
	/**
	 * Return the scene to be used for rendering
	 */
	virtual class FScene* GetRenderScene()
	{
		return NULL;
	}

	/**
	 *	Add the scene captures view info to the streaming manager
	 *
	 *	@param	StreamingManager			The streaming manager to add the view info to.
	 *	@param	View						The scene view information
	 */
	virtual void AddSceneCaptureViewInformation(FStreamingManagerCollection* StreamingManager, FSceneView* View) {};

	/**
	*	Clears Hit Mask when component is detached
	*
	*	@param	ComponentToDetach		SkeletalMeshComponent To Detach
	*/
	virtual void ClearHitMask(const UPrimitiveComponent* ComponentToDetach) {};

	/**
	*	Update Hit Mask when component is gets reattached
	*
	*	@param	ComponentToUpdate		SkeletalMeshComponent To Update
	*/
	virtual void UpdateHitMask(const UPrimitiveComponent* ComponentToUpdate) {};


	/**
	 * Dumps dynamic lighting and shadow interactions for scene to log.
	 *
	 * @param	bOnlyIncludeShadowCastingInteractions	Whether to only include shadow casting interactions
	 */
	virtual void DumpDynamicLightShadowInteractions( UBOOL bOnlyIncludeShadowCastingInteractions ) const
	{}

	virtual void DumpLightIteractions( FOutputDevice& Ar ) const { }

	/**
	 * Exports the scene.
	 *
	 * @param	Ar		The Archive used for exporting.
	 **/
	virtual void Export( FArchive& Ar ) const
	{}

protected:
	virtual ~FSceneInterface() {}
};

/**
 * Allocates a new instance of the private FScene implementation for the given world.
 * @param World - An optional world to associate with the scene.
 * @param bAlwaysAllowAudioPlayback - Indicate that audio in this scene should always be played, even if its not the GWorld.
 * @param bInRequiresHitProxies- Indicates that hit proxies should be rendered in the scene.
 */
extern FSceneInterface* AllocateScene(UWorld* World, UBOOL bAlwaysAllowAudioPlayback, UBOOL bInRequiresHitProxies);

/**
 * An interface to an actor visibility state for a single frame of a view.
 */
class FActorVisibilityHistoryInterface
{
public:

	/** Virtual destructor. */
	virtual ~FActorVisibilityHistoryInterface() {}

	/**
	 * Tests whether an actor was visible for the scene view.
	 * @param Actor - The actor to test.
	 * @return True if the actor was visible.
	 */
	virtual UBOOL GetActorVisibility(const AActor* Actor) const = 0;
};

/**
 * Encapsulates the most recent actor visibility states for a particular view.
 * Provides thread-safe access to the actor visibility states while a scene is being rendered which updates the states.
 */
class FSynchronizedActorVisibilityHistory
{
public:

	/** Initialization constructor. */
	FSynchronizedActorVisibilityHistory();

	/** Destructor. */
	~FSynchronizedActorVisibilityHistory();

	/** Initializes the history.  Must be called before calling any of the other members. */
	void Init();

	/**
	 * Tests whether an actor was visible for the scene view.
	 * @param Actor - The actor to test.
	 * @return True if the actor was visible.
	 */
	UBOOL GetActorVisibility(const AActor* Actor) const;

	/**
	 * Updates the actor visibility states for a new frame.
	 * @param NewStates - The new actor visibility states.  Ownership is transferred to the callee.
	 */
	void SetStates(FActorVisibilityHistoryInterface* NewStates);

private:

	/** The most recent actor visibility states. */
	FActorVisibilityHistoryInterface* States;

	/** The critical section used to synchronize access to the actor visibility states. */
	FCriticalSection* CriticalSection;
};

/** Parameters for temporal AA. */
struct FTemporalAAParameters
{
	FVector2D Offset;
	FLOAT StartDepth;

	/** Default constructor. */
	FTemporalAAParameters(): Offset(0,0), StartDepth(0) {}

	/** Returns the parameters packed into a vector. */
	FVector GetVector() const
	{
		return FVector(Offset.X, Offset.Y, StartDepth);
	}
};

/** Calculates temporal AA parameters for a given view size and velocity. */
extern FTemporalAAParameters CalcTemporalAAParameters(UBOOL bAllowTemporalAA, UINT SizeX, UINT SizeY, FLOAT Velocity);

/**
 * A projection from scene space into a 2D screen region.
 */
class FSceneView
{
public:
	const FSceneViewFamily* Family;
	FSceneViewStateInterface* State;

	/**
	 *	The index into the ParentViewFamily array indicating which view is 'active'.
	 *	If -1, it indicates that the operation should iterate over the ParentViews itself.
	 */
	INT ParentViewIndex;
	/** 
	 *	The parent view family of the main scene - used for scene capture objects like portals.
	 */
	const FSceneViewFamily* ParentViewFamily;

	/** A pointer to the history to write the view's actor visibility states to.  May be NULL. */
	FSynchronizedActorVisibilityHistory* ActorVisibilityHistory;

	/** The actor which is being viewed from. */
	const AActor* ViewActor;

	/** Chain of post process effects for this view */
	const UPostProcessChain* PostProcessChain;

	/** The view-specific post process settings. */
	const struct FPostProcessSettings* PostProcessSettings;

	/** 
	 * Stores post process scene proxies for use instead of creating them via the post process chain.
	 *
	 * @note	This array is needed by scene capture objects which create their proxies on the game thread.
	 */
	TArray<class FPostProcessSceneProxy*> PostProcessSceneProxies;

	/** An interaction which draws the view's interaction elements. */
	FViewElementDrawer* Drawer;

	// final position of the view
	// should not have fractional part but there is coding using appTrunc()
	// FLOAT to avoid int->float

	/* Absolute X coordinate of the view in pixels */
	FLOAT X;
	/* Absolute Y coordinate of the view in pixels */
	FLOAT Y;
	/* Relative X coordinate of the clipping space of the view in pixels 
	   Does not include the black bars */
	FLOAT ClipX;	
	/* Relative Y coordinate of the clipping space of the view in pixels
	   Does not include the black bars */
	FLOAT ClipY;
	/* Horizontal size of the view in pixels */
	FLOAT SizeX;
	/* Vertical size of the view in pixels */
	FLOAT SizeY;

	// use this during intermediate rendering passes (non final view)
	// Computed from X, Y, SizeX, SizeY
	// Relative to MinFamilyX, MinFamilyY
	// Clamped into the render target.

	/** The X coordinate of the view in the scene render targets. */
	INT RenderTargetX;
	/** The Y coordinate of the view in the scene render targets. */
	INT RenderTargetY;
	/** The width of the view in the scene render targets (name is misleading). */
	INT RenderTargetSizeX;
	/** The height of the view in the scene render targets (name is misleading). */
	INT RenderTargetSizeY;

	/** 
	 * Copy from GFrameNumber
	 */
	UINT FrameNumber;

	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;

	FLinearColor BackgroundColor;
	FLinearColor OverlayColor;

	/** Color scale multiplier used during post processing */
	FLinearColor ColorScale;

	FVector4 DiffuseOverrideParameter;
	FVector4 SpecularOverrideParameter;

	/** The primitives which are hidden for this view. */
	TSet<UPrimitiveComponent*> HiddenPrimitives;

	/** Per-frame random 2D offset for screen door dissolves */
	FVector2D ScreenDoorRandomOffset;

	/** Gets updated every frame if there is a DOF post processsing proxy */
	FDepthOfFieldParams DepthOfFieldParams;

	// Derived members.

	/** The view transform, starting from world-space points translated by -ViewOrigin. */
	FMatrix TranslatedViewMatrix;

	/** The view-projection transform, starting from world-space points translated by -ViewOrigin. */
	FMatrix TranslatedViewProjectionMatrix;

	/** The inverse view-projection transform, ending with world-space points translated by -ViewOrigin. */
	FMatrix InvTranslatedViewProjectionMatrix;

	/** The translation to apply to the world before TranslatedViewProjectionMatrix. */
	FVector PreViewTranslation;

	/** The temporal AA parameters. */
	FTemporalAAParameters TemporalAAParameters;

	FMatrix ViewProjectionMatrix;
	FMatrix InvProjectionMatrix;
	FMatrix InvViewMatrix;
	FMatrix InvViewProjectionMatrix;
	FVector4 ViewOrigin;
	FConvexVolume ViewFrustum;
	UBOOL bHasNearClippingPlane;
	FPlane NearClippingPlane;
	FLOAT NearClippingDistance;

	/** TRUE if ViewMatrix.Determinant() is negative. */
	UBOOL bReverseCulling;

	/* Vector used by shaders to convert depth buffer samples into z coordinates in world space */
	FVector4 InvDeviceZToWorldZTransform;

	/* Vector used by shaders to convert projection-space coordinates to texture space. */
	FVector4 ScreenPositionScaleBias;

	/** FOV based multiplier for cull distance on objects */
	FLOAT LODDistanceFactor;
	/** Square of the FOV based multiplier for cull distance on objects */
	FLOAT LODDistanceFactorSquared;

	/** Whether we did a camera cut for this view this frame. */
	UBOOL bCameraCut;

	/** Whether the scene color is contained in the LDR surface */
	UBOOL bUseLDRSceneColor;

	/** True if GIsGame was True when this view was created. */
	UBOOL bIsGameView;

	/** TRUE if only the lowest detail massive LODs should be drawn for this view. */
	UBOOL bForceLowestMassiveLOD;

	/** Whether to render Temporal AA or not for this view.  This is a cached value calculated off of the show flags, RenderingOverrides, etc. */
	UBOOL bRenderTemporalAA;

	/** Whether to force a clear of the color target for this view.  When FALSE, some platforms can skip the clear as an optimization */
	UBOOL bForceClear;

	/** Allocates channels of the light attenuation texture for dominant light shadows each frame. */
	FLightChannelAllocator DominantLightChannelAllocator;

	/** Rendering overrides for various features, specific to this view. */
	FRenderingPerformanceOverrides RenderingOverrides;

#if !CONSOLE
	/** The set of (the first 64) groups' visibility info for this view */
	QWORD EditorViewBitflag;

	/** For ortho views, this can control how to determine LOD parenting (ortho has no "distance-to-camera") */
	FVector4 OverrideLODViewOrigin;

	/** True if we should draw translucent objects when rendering hit proxies */
	UBOOL bAllowTranslucentPrimitivesInHitProxy;

	/** BitArray representing the visibility state of the various sprite categories in the editor for this view */
	TBitArray<> SpriteCategoryVisibility;
#endif

#if WITH_REALD
	UBOOL bRealDStereoEnabled;
	FVector4 RealDCoefficients;
	FVector4 DBATempVariables;
#endif
 
	/** Initialization constructor. */
	FSceneView(
		const FSceneViewFamily* InFamily,
		FSceneViewStateInterface* InState,
		INT InParentViewIndex,
		const FSceneViewFamily* InParentViewFamily,
		FSynchronizedActorVisibilityHistory* InActorVisibilityHistory,
		const AActor* InViewActor,
		const UPostProcessChain* InPostProcessChain,
		const FPostProcessSettings* InPostProcessSettings,
		FViewElementDrawer* InDrawer,
		FLOAT InX,
		FLOAT InY,
		FLOAT InSizeX,
		FLOAT InSizeY,
		const FMatrix& InViewMatrix,
		const FMatrix& InProjectionMatrix,
		const FLinearColor& InBackgroundColor,
		const FLinearColor& InOverlayColor,
		const FLinearColor& InColorScale,
		const TSet<UPrimitiveComponent*>& InHiddenPrimitives,
		const FRenderingPerformanceOverrides& RenderingOverrides = FRenderingPerformanceOverrides(E_ForceInit),
		FLOAT InLODDistanceFactor = 1.0f,
		UBOOL bInCameraCut = FALSE,
		FTemporalAAParameters InTemporalAAParameters = FTemporalAAParameters()
#if !CONSOLE
		, QWORD InEditorViewBitflag=1 // default to 0'th view index, which is a bitfield of 1
		, const FVector4& InOverrideLODViewOrigin=FVector4(0,0,0,0) // this can be specified for ortho views so that it's min draw distance/LOD parenting etc, is controlled by a perspective viewport

		, UBOOL InUseFauxOrthoViewPos = FALSE // In case of ortho, generate a fake view position that has a non-zero W component. The view position will be derived based on the view matrix.
		, const TBitArray<>& InSpriteCategoryVisibility=TBitArray<>()
#endif
		);

	FSceneView(
		const FSceneViewFamily* InFamily,
		FSceneViewStateInterface* InState,
		INT InParentViewIndex,
		const FSceneViewFamily* InParentViewFamily,
		FSynchronizedActorVisibilityHistory* InActorVisibilityHistory,
		const AActor* InViewActor,
		const UPostProcessChain* InPostProcessChain,
		const FPostProcessSettings* InPostProcessSettings,
		FViewElementDrawer* InDrawer,
		FLOAT InX,
		FLOAT InY,
		FLOAT InClipX,
		FLOAT InClipY,
		FLOAT InSizeX,
		FLOAT InSizeY,
		const FMatrix& InViewMatrix,
		const FMatrix& InProjectionMatrix,
		const FLinearColor& InBackgroundColor,
		const FLinearColor& InOverlayColor,
		const FLinearColor& InColorScale,
		const TSet<UPrimitiveComponent*>& InHiddenPrimitives,
		const FRenderingPerformanceOverrides& RenderingOverrides = FRenderingPerformanceOverrides(E_ForceInit),
		FLOAT InLODDistanceFactor = 1.0f,
		UBOOL bInCameraCut = FALSE,
		FTemporalAAParameters InTemporalAAParameters = FTemporalAAParameters()
#if !CONSOLE
		, QWORD InEditorViewBitflag=1 // default to 0'th view index, which is a bitfield of 1
		, const FVector4& InOverrideLODViewOrigin=FVector4(0,0,0,0) // this can be specified for ortho views so that it's min draw distance/LOD parenting etc, is controlled by a perspective viewport

		, UBOOL InUseFauxOrthoViewPos = FALSE // In case of ortho, generate a fake view position that has a non-zero W component. The view position will be derived based on the view matrix.
		, const TBitArray<>& InSpriteCategoryVisibility=TBitArray<>()
#endif
		);

	/** Transforms a point from world-space to the view's screen-space. */
	FVector4 WorldToScreen(const FVector& WorldPoint) const;

	/** Transforms a point from the view's screen-space to world-space. */
	FVector ScreenToWorld(const FVector4& ScreenPoint) const;

	/** Transforms a point from the view's screen-space into pixel coordinates relative to the view's X,Y. */
	UBOOL ScreenToPixel(const FVector4& ScreenPoint,FVector2D& OutPixelLocation) const;

	/** Transforms a point from pixel coordinates relative to the view's X,Y (left, top) into the view's screen-space. */
	FVector4 PixelToScreen(FLOAT X,FLOAT Y,FLOAT Z) const;

	/** Transforms a point from the view's world-space into pixel coordinates relative to the view's X,Y (left, top). */
	UBOOL WorldToPixel(const FVector& WorldPoint,FVector2D& OutPixelLocation) const;

	/** 
	 * Transforms a point from the view's world-space into the view's screen-space. 
	 * Divides the resulting X, Y, Z by W before returning. 
	 */
	FPlane Project(const FVector& WorldPoint) const;

	/** 
	 * Transforms a point from the view's screen-space into world coordinates
	 * multiplies X, Y, Z by W before transforming. 
	 */
	FVector Deproject(const FPlane& ScreenPoint) const;

	/** transforms 2D screen coordinates into a 3D world-space origin and direction 
	 * @param ScreenPos - screen coordinates in pixels
	 * @param out_WorldOrigin (out) - world-space origin vector
	 * @param out_WorldDirection (out) - world-space direction vector
	 */
	void DeprojectFVector2D(const FVector2D& ScreenPos, FVector& out_WorldOrigin, FVector& out_WorldDirection);

	inline FVector GetViewDirection() const { return ViewMatrix.GetColumn(2); }

	/** @return true:perspective, false:orthographic */
	inline UBOOL IsPerspectiveProjection() const { return ViewOrigin.W > 0.0f; }

	/** 
	 * Encodes world space depth into the value stored in scene color alpha. 
	 * Note: This must match EncodeFloatW in Common.usf!
	 */
	inline FLOAT EncodeFloatW(FLOAT W) const
	{
		checkSlow(!GSupportsDepthTextures);
		float DepthAdd = -InvDeviceZToWorldZTransform.X;
		float DepthMul = 1 - InvDeviceZToWorldZTransform.Y;
		return DepthMul + DepthAdd / W;
	}
	/** 
	 * Decodes the value stored in scene color alpha into world space depth. 
	 * Note: This must match DecodeFloatW in Common.usf!
	 */
	inline FLOAT DecodeFloatW(FLOAT EncodedW) const
	{
		checkSlow(!GSupportsDepthTextures);
		FLOAT DepthAdd = -InvDeviceZToWorldZTransform.X;
		FLOAT DepthMul = 1 - InvDeviceZToWorldZTransform.Y;
		return DepthAdd / (EncodedW - DepthMul);
	}

private:

#if !CONSOLE
	void Initialize(UBOOL InUseFauxOrthoViewPos);
#else
	void Initialize();
#endif
	
};


/**
 * A set of views into a scene which only have different view transforms and owner actors.
 */
class FSceneViewFamily
{
public:

	/** The views which make up the family. */
	TArray<const FSceneView*> Views;

	/** The render target which the views are being rendered to. */
	const FRenderTarget* RenderTarget;

	/** The scene being viewed. */
	FSceneInterface* Scene;

	/** The show flags for the views. */
	EShowFlags ShowFlags;

	/** The current world time. */
	FLOAT CurrentWorldTime;

	/** The difference between the last world time and CurrentWorldTime. */
	FLOAT DeltaWorldTime;

	/** The current real time. */
	FLOAT CurrentRealTime;

	/** Indicates whether the view family is updated in realtime. */
	UBOOL bRealtimeUpdate;

	/** Indicates whether ambient occlusion is allowed on any views in this view family. */
	UBOOL bAllowAmbientOcclusion;

	/** Used to defer the back buffer clearing to just before the back buffer is drawn to */
	UBOOL bDeferClear;

	/** if TRUE then the scene color is not cleared when rendering this set of views. */
	UBOOL bClearScene;

	/** if TRUE then results of scene rendering are copied/resolved to the RenderTarget. */
	UBOOL bResolveScene;

	/** If TRUE, then the opacity channel will be written out to the destination alpha */
	UBOOL bWriteOpacityToAlpha;

	/** If TRUE, then the renderer will be forced to write its results to the RenderTarget */
	UBOOL bScreenCaptureRenderTarget;

	/** Gamma correction used when rendering this family. Default is 1.0 */
	FLOAT GammaCorrection;

#if !CONSOLE
	/** Indicates whether, of not, the base attachment volume should be drawn. */
	UBOOL bDrawBaseInfo;
#endif

	/** Initialization constructor. */
	FSceneViewFamily(
		const FRenderTarget* InRenderTarget,
		FSceneInterface* InScene,
		EShowFlags InShowFlags,
		FLOAT InCurrentWorldTime,
		FLOAT InDeltaWorldTime,
		FLOAT InCurrentRealTime,
		UBOOL InbRealtimeUpdate,
		UBOOL InbAllowAmbientOcclusion,
		UBOOL InbDeferClear,
		UBOOL InbClearScene,
		UBOOL InbResolveScene,
		FLOAT InGammaCorrection,
 		UBOOL InbWriteOpacityToAlpha,
		UBOOL InbScreenCaptureRenderTarget
		);

	inline UBOOL ShouldPostProcess() const
	{
#if MOBILE || WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			return GMobileAllowPostProcess;
		}
#endif
		return TRUE;
	}

	UBOOL ShouldDrawShadows() const;
};

/**
 * Call from the game thread to send a message to the rendering thread to being rendering this view family.
 */
extern void BeginRenderingViewFamily(FCanvas* Canvas,const FSceneViewFamily* ViewFamily);

/**
 * Call from the game thread to send a message to the rendering thread to render the previously rendered scene color (as a replacement to
 * rendering the view family
 */
extern void RenderCapturedSceneColor(FCanvas* Canvas,const FSceneViewFamily* ViewFamily);

/**
 * A view family which deletes its views when it goes out of scope.
 */
class FSceneViewFamilyContext : public FSceneViewFamily
{
public:
	/** Initialization constructor. */
	FSceneViewFamilyContext(
		const FRenderTarget* InRenderTarget,
		FSceneInterface* InScene,
		EShowFlags InShowFlags,
		FLOAT InCurrentWorldTime,
		FLOAT InDeltaWorldTime,
		FLOAT InCurrentRealTime,
		UBOOL InbRealtimeUpdate=FALSE,
		UBOOL InbAllowAmbientOcclusion=FALSE,
		UBOOL InbDeferClear=FALSE,
		UBOOL InbClearScene=TRUE,
		UBOOL InbResolveScene=TRUE,
		FLOAT InGammaCorrection=1.0f,
		UBOOL InbWriteOpacityToAlpha=FALSE,
		UBOOL InbScreenCaptureRenderTarget=FALSE)
		:	FSceneViewFamily(
		InRenderTarget,
		InScene,
		InShowFlags,
		InCurrentWorldTime,
		InDeltaWorldTime,
		InCurrentRealTime,
		InbRealtimeUpdate,
		InbAllowAmbientOcclusion,
		InbDeferClear,
		InbClearScene,
		InbResolveScene,
		InGammaCorrection,
		InbWriteOpacityToAlpha,
		InbScreenCaptureRenderTarget
		)
	{}
	/** Destructor. */
	~FSceneViewFamilyContext();
};

// Static-function 
class FPrimitiveSceneProxy;
FPrimitiveSceneProxy* Scene_GetProxyFromInfo(FPrimitiveSceneInfo* Info);

