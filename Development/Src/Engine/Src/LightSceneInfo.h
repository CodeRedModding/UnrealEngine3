/*=============================================================================
	LightSceneInfo.h: Light scene info definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LIGHTSCENEINFO_H__
#define __LIGHTSCENEINFO_H__

#include "StaticArray.h"

/** An interface to the information about a light's effect on a scene's DPG. */
class FLightSceneDPGInfoInterface
{
public:

	enum ELightPassDrawListType
	{
		ELightPass_Default=0,
		ELightPass_Decals,
		ELightPass_MAX
	};

	virtual UBOOL DrawStaticMeshesVisible(
		const FViewInfo& View,
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
		ELightPassDrawListType DrawType
		) const = 0;

	virtual ELightInteractionType AttachStaticMesh(const FLightSceneInfo* LightSceneInfo,FStaticMesh* Mesh) = 0;

	virtual void DetachStaticMeshes() = 0;

	virtual UBOOL DrawDynamicMesh(
		const FSceneView& View,
		const FLightSceneInfo* LightSceneInfo,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		) const = 0;
};

/** An interface for rendering lights for translucent objects */
class FTranslucencyLightDrawInterface
{
public:
	virtual UBOOL DrawDynamicMesh(
		const FSceneView& View,
		const FLightSceneInfo* LightSceneInfo,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		) const = 0;
};


/**
 * The information needed to cull a light-primitive interaction.
 */
class FLightSceneInfoCompact
{
public:

	FLightSceneInfo* LightSceneInfo;
	const ULightEnvironmentComponent* LightEnvironment;
	FLightingChannelContainer LightingChannels;
	VectorRegister BoundingSphereVector;
	FLinearColor Color;
	BITFIELD bStaticShadowing : 1;
	BITFIELD bCastDynamicShadow : 1;
	BITFIELD bCastStaticShadow : 1;
	BITFIELD bProjectedShadows : 1;
	BITFIELD bStaticLighting : 1;
	BITFIELD bCastCompositeShadow : 1;


	/** Initializes the compact scene info from the light's full scene info. */
	void Init(FLightSceneInfo* InLightSceneInfo);

	/** Default constructor. */
	FLightSceneInfoCompact():
		LightSceneInfo(NULL),
		LightEnvironment(NULL)
	{}

	/** Initialization constructor. */
	FLightSceneInfoCompact(FLightSceneInfo* InLightSceneInfo)
	{
		Init(InLightSceneInfo);
	}

	/**
	 * Tests whether this light affects the given primitive.  This checks both the primitive and light settings for light relevance
	 * and also calls AffectsBounds.
	 *
	 * @param CompactPrimitiveSceneInfo - The primitive to test.
	 * @return True if the light affects the primitive.
	 */
	UBOOL AffectsPrimitive(const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo) const;

	/**
	* Tests whether this light's modulated shadow affects the given primitive by doing a bounds check.
	*
	* @param CompactPrimitiveSceneInfo - The primitive to test.
	* @return True if the modulated shadow affects the primitive.
	*/
	UBOOL AffectsModShadowPrimitive(const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo) const;
};

/** The type of the octree used by FScene to find lights. */
typedef TOctree<FLightSceneInfoCompact,struct FLightOctreeSemantics> FSceneLightOctree;

/**
 * The information used to render a light.  This is the rendering thread's mirror of the game thread's ULightComponent.
 */
class FLightSceneInfo
{
public:
    /** The light component. */
    const ULightComponent* LightComponent;

	/** The light's persistent shadowing GUID. */
	FGuid LightGuid;

	/** The light's persistent lighting GUID. */
	FGuid LightmapGuid;

	/** A transform from world space into light space. */
	FMatrix WorldToLight;

	/** A transform from light space into world space. */
	FMatrix LightToWorld;

	/** The homogenous position of the light. */
	FVector4 Position;

	/** The light color. */
	FLinearColor Color;

	/** The light channels which this light affects. */
	const FLightingChannelContainer LightingChannels;

	/** The list of static primitives affected by the light. */
	FLightPrimitiveInteraction* StaticPrimitiveList;

	/** The list of dynamic primitives affected by the light. */
	FLightPrimitiveInteraction* DynamicPrimitiveList;

#if USE_MASSIVE_LOD
	/** The list of primitives waiting for their parents to be attached. */
	TMultiMap<UPrimitiveComponent*, FLightPrimitiveInteraction*> OrphanedPrimitiveMap;
#endif

	/** The index of the primitive in Scene->Lights. */
	INT Id;

	/** The identifier for the primitive in Scene->PrimitiveOctree. */
	FOctreeElementId OctreeId;

	/** Light function parameters. */
	FVector	LightFunctionScale;
	FLOAT LightFunctionDisabledBrightness;
	const FMaterialRenderProxy* LightFunction;
	/** Bound shader state for this light's light function. This is mutable because it is cached on first use, possibly when const */
	mutable FBoundShaderStateRHIRef LightFunctionBoundShaderState; 

	/** True if the light will cast projected shadows from dynamic primitives. */
	const BITFIELD bProjectedShadows : 1;

	/** True if primitives will cache static lighting for the light. */
	const BITFIELD bStaticLighting : 1;

	/** True if primitives will cache static shadowing for the light. */
	const BITFIELD bStaticShadowing : 1;

	/** True if the light casts dynamic shadows. */
	const BITFIELD bCastDynamicShadow : 1;

	/** True if the light should cast shadow from primitives which use a composite light environment. */
	const BITFIELD bCastCompositeShadow : 1;

	/** True if the light casts static shadows. */
	const BITFIELD bCastStaticShadow : 1;

	/** If enabled and the light casts modulated shadows, this will cause self-shadowing of shadows rendered from this light to use normal shadow blending. */
	const BITFIELD bNonModulatedSelfShadowing : 1;

	/** Causes shadows from this light to only self shadow. */
	const BITFIELD bSelfShadowOnly : 1;

	/** Whether to allow preshadows (the static environment casting dynamic shadows on dynamic objects) from this light. */
	const BITFIELD bAllowPreShadow : 1;

	/** Whether the light's owner is selected in the editor. */
	BITFIELD bOwnerSelected : 1;

	/** True if the light is built. */
	BITFIELD bPrecomputedLightingIsValid : 1;

	/** Whether this light is being used as the OverrideLightComponent on a primitive and shouldn't affect any other primitives. */
	BITFIELD bExplicitlyAssignedLight : 1;

	/** Whether this light can be combined into the DLE normally.  Overridden to false in the case of muzzle flashes to prevent SH artifacts */
	BITFIELD bAllowCompositingIntoDLE : 1;

	/** Whether to render light shafts from this light. */
	BITFIELD bRenderLightShafts : 1;

	/** The light environment which the light is in. */
	const ULightEnvironmentComponent* LightEnvironment;

	/** List of primitive components that are allowed to receive forward shadows from this light, used to cut down on draw calls. */
	TArray<UPrimitiveComponent*> ForwardShadowReceivers;

	/** The light type (ELightComponentType) */
	const BYTE LightType;

	/** Type of shadowing to apply for the light (ELightShadowMode) */
	BYTE LightShadowMode;

	/** Type of shadow projection to use for this light */
	const BYTE ShadowProjectionTechnique;

	/** Quality of shadow buffer filtering to use for this light's shadows */
	const BYTE ShadowFilterQuality;

	/**
	 * Override for min dimensions (in texels) allowed for rendering shadow subject depths.
	 * This also controls shadow fading, once the shadow resolution reaches MinShadowResolution it will be faded out completely.
	 * A value of 0 defaults to MinShadowResolution in SystemSettings.
	 */
	const INT MinShadowResolution;

	/**
	 * Override for max square dimensions (in texels) allowed for rendering shadow subject depths.
	 * A value of 0 defaults to MaxShadowResolution in SystemSettings.
	 */
	const INT MaxShadowResolution;

	/** 
	 * Resolution in texels below which shadows begin to be faded out. 
	 * Once the shadow resolution reaches MinShadowResolution it will be faded out completely.
	 * A value of 0 defaults to ShadowFadeResolution in SystemSettings.
	 */
	const INT ShadowFadeResolution;

	/** Number of dynamic interactions with statically lit primitives. */
	INT NumUnbuiltInteractions;

	/** The name of the level the light is in. */
	FName LevelName;

	/** Shadow color for modulating entire scene */
	FLinearColor ModShadowColor;

	/** Time since the subject was last visible at which the mod shadow will fade out completely.  */
	FLOAT ModShadowFadeoutTime;

	/** Exponent that controls mod shadow fadeout curve. */
	FLOAT ModShadowFadeoutExponent;

	/** Uniform penumbra size for distance field shadow mapped lights. */
	FLOAT DistanceFieldShadowMapPenumbraSize;

	/** Shadow falloff exponent for distance field shadow mapped lights. */
	FLOAT DistanceFieldShadowMapShadowExponent;

	/** Everything closer to the camera than this distance will occlude light shafts. */
	FLOAT OcclusionDepthRange;

	/** Scales the additive color. */
	FLOAT BloomScale;

	/** Scene color must be larger than this to create bloom in the light shafts. */
	FLOAT BloomThreshold;

	/** 
	 * Scene color luminance must be less than this to receive bloom from light shafts. 
	 * This behaves like Photoshop's screen blend mode and prevents over saturation from adding bloom to already bright areas.
	 * The default value of 1 means that a pixel with a luminance of 1 won't receive any bloom, but a luminance of .5 will receive half bloom.
	 */
	FLOAT BloomScreenBlendThreshold;

	/** Multiplies against scene color to create the bloom color. */
	FColor BloomTint;

	/** 100 is maximum blur length, 0 is no blur. */
	FLOAT RadialBlurPercent;

	/** Controls how dark the occlusion masking is, a value of .5 would mean that an occlusion of 0 only darkens underlying color by half. */
	FLOAT OcclusionMaskDarkness;

	FName LightComponentName;
	FName GetLightName() const { return LightComponentName; }

	/** The scene the light is in. */
	FScene* Scene;

	// Accessors.
	FVector GetDirection() const { return FVector(WorldToLight.M[0][2],WorldToLight.M[1][2],WorldToLight.M[2][2]); }
	FVector GetOrigin() const { return LightToWorld.GetOrigin(); }
	FVector4 GetPosition() const { return Position; }
	FORCEINLINE FBoxCenterAndExtent GetBoundingBox() const
	{
		const FLOAT Extent = GetRadius();
		return FBoxCenterAndExtent(
			GetOrigin(),
			FVector(Extent,Extent,Extent)
			);
	}

	virtual FSphere GetBoundingSphere() const
	{
		// Directional lights will have a radius of WORLD_MAX
		return FSphere(GetPosition(), Min(GetRadius(), (FLOAT)WORLD_MAX));
	}

	virtual FLinearColor GetDirectIntensity(const FVector& Point) const;

	/** Composites the light's influence into an SH vector. */
	virtual void CompositeInfluence(const FVector& Point, FSHVectorRGB& CompositeSH) const;

	virtual const FSHVectorRGB* GetSHIncidentLighting() const
	{
		return NULL;
	}

	virtual FPlane GetShadowPlane() const { return FPlane(0,0,1,0); }

	/** @return radius of the light */
	virtual FLOAT GetRadius() const { return FLT_MAX; }

	virtual FLOAT GetOuterConeAngle() const { return 0; }

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FVector4& LightPositionAndInvRadius, FVector4& LightColorAndFalloffExponent, FVector& LightDirection, FVector2D& SpotAngles) const {}
	
	/** Initialization constructor. */
	FLightSceneInfo(const ULightComponent* InLight);
	virtual ~FLightSceneInfo() {}

	/** Adds the light to the scene. */
	void AddToScene();

	/**
	 * If the light affects the primitive, create an interaction, and process children 
	 * 
	 * @param LightSceneInfoCompact Compact representation of the light
	 * @param PrimitiveSceneInfoCompact Compact representation of the primitive
	 */
	void CreateLightPrimitiveInteraction(const FLightSceneInfoCompact& LightSceneInfoCompact, const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact);

	/** Removes the light from the scene. */
	void RemoveFromScene();

	/** Detaches the light from the primitives it affects. */
	void Detach();

	/**
	 * Tests whether the light affects the given bounding volume.
	 * @param Bounds - The bounding volume to test.
	 * @return True if the light affects the bounding volume
	 */
	virtual UBOOL AffectsBounds(const FBoxSphereBounds& Bounds) const
	{
		return TRUE;
	}

	virtual void DetachPrimitive(const FLightPrimitiveInteraction& Interaction) {}
	virtual void AttachPrimitive(const FLightPrimitiveInteraction& Interaction) {}

protected:

	void DetachPrimitiveShared(const FLightPrimitiveInteraction& Interaction);
	void AttachPrimitiveShared(const FLightPrimitiveInteraction& Interaction);

	inline BYTE GetEffectiveShadowFilterQuality(UBOOL bRenderingBeforeLight) const
	{
		// Bias the shadow quality down for the modulated part of a shadow whose self shadowing is using normal shadow blending
		return bNonModulatedSelfShadowing && !bRenderingBeforeLight ? Max(ShadowFilterQuality - 1, 0) : ShadowFilterQuality;
	}

public:

	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual UBOOL GetWholeSceneProjectedShadowInitializer(const TArray<FViewInfo>& Views, TArray<class FProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
	{
		return FALSE;
	}

	/** Called at the beginning of the frame in the editor. */
	virtual void BeginSceneEditor() {}

	/** Returns the number of view dependent shadows this light will create. */
	virtual INT GetNumViewDependentWholeSceneShadows() const { return 0; }

	/** Returns TRUE if this light is both directional and has a valid light function */
	virtual UBOOL IsDirectionLightWithLightFunction() const { return FALSE; }

	/**
	 * Sets up a projected shadow initializer that's dependent on the current view for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual UBOOL GetViewDependentWholeSceneProjectedShadowInitializer(
		const FViewInfo& View, 
		INT SplitIndex,
		class FProjectedShadowInitializer& OutInitializer) const
	{
		return FALSE;
	}

	virtual FSphere GetShadowSplitBounds(const FViewInfo& View, INT SplitIndex) const { return FSphere(FVector(0,0,0), 0); }

	/**
	 * Sets up a projected shadow initializer for the given subject.
	 * @param SubjectBounds - The bounding volume of the subject.
	 * @param OutInitializer - Upon successful return, contains the initialization parameters for the shadow.
	 * @return True if a projected shadow should be cast by this subject-light pair.
	 */
	virtual UBOOL GetProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,class FProjectedShadowInitializer& OutInitializer) const
	{
		return FALSE;
	}

	virtual void SetDepthBounds(const FSceneView* View) const
	{
	}

	virtual void SetScissorRect(const FSceneView* View) const
	{
	}

	/**
	 * Returns a pointer to the light type's DPG info object for the given DPG.
	 * @param DPGIndex - The index of the DPG to get the info object for.
	 * @return The DPG info interface.
	 */
	virtual const FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex) const = 0;
	virtual FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex) = 0;

	virtual const class FPointLightSceneInfoBase* GetPointLightInfo() const { return NULL; }
	virtual const class FSpotLightSceneInfo* GetSpotLightInfo() const { return NULL; }

	/** Returns TRUE if distance fading is enabled and passes out distance fade parameters in DistanceFadeValues. */
	virtual UBOOL GetDirectionalLightDistanceFadeParameters(FVector2D& DistanceFadeValues) const { return FALSE; }

	virtual UBOOL DrawTranslucentMesh(
		const FSceneView& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		UBOOL bUseTranslucencyLightAttenuation,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const class FProjectedShadowInfo* TranslucentPreShadowInfo,
		FHitProxyId HitProxyId
		) const = 0;

	/**
	* @return modulated shadow projection pixel shader for this light type
	*/
	virtual class FShadowProjectionPixelShaderInterface* GetModShadowProjPixelShader(UBOOL bRenderingBeforeLight) const = 0;
	
	/**
	* @return Branching PCF modulated shadow projection pixel shader for this light type
	*/
	virtual class FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShader(UBOOL bRenderingBeforeLight) const = 0;

	/**
	* @return modulated shadow projection bound shader state for this light type
	*/
	virtual FGlobalBoundShaderState* GetModShadowProjBoundShaderState(UBOOL bRenderingBeforeLight) const = 0;
	/** Bound shader state for this light's modulated shadow projection. This is mutable because it is cached on first use, possibly when const */
	mutable TStaticArray<FGlobalBoundShaderState,SFQ_Num> ModShadowProjBoundShaderStates;

	/**
	* @return PCF Branching modulated shadow projection bound shader state for this light type
	*/
	virtual FGlobalBoundShaderState* GetBranchingPCFModProjBoundShaderState(UBOOL bRenderingBeforeLight) const = 0;
	/** Bound shader state for this light's PCF Branching modulated shadow projection. This is mutable because it is cached on first use, possibly when const */
	mutable TStaticArray<FGlobalBoundShaderState,SFQ_Num> ModBranchingPCFBoundShaderStates;

	/** Hash function. */
	friend DWORD GetTypeHash(const FLightSceneInfo* LightSceneInfo)
	{
		return (DWORD)LightSceneInfo->Id;
	}

	/** Returns whether the light can be rendered in a deferred pass. */
	inline UBOOL SupportsDeferredShading() const
	{
		// SH lights not supported deferred for now
		return LightType != LightType_SphericalHarmonic
			// Lights with static shadowing can't be applied deferred because they need a shadowmap coordinate
			&& !bStaticShadowing
			// Skip light environment lights since they support several features that can't be done deferred
			&& !LightEnvironment
			// Only allow deferring lights with channels that are supported by the channel mask
			&& LightingChannels.GetDeferredShadingChannelMask() > 0;
	}
};

/** Defines how the light is stored in the scene's light octree. */
struct FLightOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(const FLightSceneInfoCompact& Element)
	{
		return Element.LightSceneInfo->GetBoundingBox();
	}

	FORCEINLINE static UBOOL AreElementsEqual(const FLightSceneInfoCompact& A,const FLightSceneInfoCompact& B)
	{
		return A.LightSceneInfo == B.LightSceneInfo;
	}
	
	FORCEINLINE static void SetElementId(const FLightSceneInfoCompact& Element,FOctreeElementId Id)
	{
		Element.LightSceneInfo->OctreeId = Id;
	}
};

#endif
