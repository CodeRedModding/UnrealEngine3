/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class Declaration

class SpeedTreeComponent extends PrimitiveComponent 
	native(SpeedTree)
	AutoExpandCategories(Collision,Rendering,Lighting)
	dependson(LightmassPrimitiveSettingsObject)
	hidecategories(Object)
	editinlinenew;
	
/** 
 * Enumerates the types of meshes in a SpeedTreeComponent. 
 * Note: This is mirrored in Lightmass.
 */
enum ESpeedTreeMeshType
{
	// Have to use a min that is one less than the real min, 
	// Since we can't assign enums to other enums in UnrealScript
	STMT_MinMinusOne,
	STMT_Branches1,
	STMT_Branches2,
	STMT_Fronds,
	STMT_LeafCards,
	STMT_LeafMeshes,
	STMT_Billboards,
	STMT_Max
};

/** USpeedTree resource.																				*/
var(SpeedTree)	const	SpeedTree				SpeedTree;

// Flags
/** Whether to draw leaf cards or not.																	*/
var(SpeedTree) 			bool 					bUseLeafCards;
/** Whether to draw leaf meshes or not.																	*/
var(SpeedTree) 			bool 					bUseLeafMeshes;
/** Whether to draw branches or not.																	*/
var(SpeedTree) 			bool 					bUseBranches;		
/** Whether to draw fronds or not.																		*/
var(SpeedTree) 			bool 					bUseFronds;
/** Whether billboards are drawn at the lowest LOD or not.												*/
var(SpeedTree)			bool					bUseBillboards;				

// LOD 
/** The distance for the most detailed tree.															*/
var(SpeedTree)			float					Lod3DStart;
/** The distance for the lowest detail tree.															*/
var(SpeedTree)			float					Lod3DEnd;
/** The distance for the most detailed tree.															*/
var(SpeedTree)			float					LodBillboardStart;
/** The distance for the lowest detail tree.															*/
var(SpeedTree)			float					LodBillboardEnd;
/** the tree will use this LOD level (0.0 - 1.0). If -1.0, the tree will calculate its LOD normally.	*/
var(SpeedTree)			float					LodLevelOverride;

// Material overrides

/** Branch material. */
var(SpeedTree) MaterialInterface		Branch1Material;

/** Cap material. */
var(SpeedTree) MaterialInterface		Branch2Material;

/** Frond material. */
var(SpeedTree) MaterialInterface		FrondMaterial;

/** Leaf card material. */
var(SpeedTree) MaterialInterface		LeafCardMaterial;

/** Leaf mesh material. */
var(SpeedTree) MaterialInterface		LeafMeshMaterial;

/** Billboard material. */
var(SpeedTree) MaterialInterface		BillboardMaterial;

// Internal
/** Icon texture. */
var	editoronly private Texture2D				SpeedTreeIcon;

/** The static lighting for a single light's affect on the component. */
struct native SpeedTreeStaticLight
{
	var private const Guid Guid;
	var private const ShadowMap1D BranchShadowMap;
	var private const ShadowMap1D FrondShadowMap;
	var private const ShadowMap1D LeafMeshShadowMap;
	var private const ShadowMap1D LeafCardShadowMap;
	var private const ShadowMap1D BillboardShadowMap;
};

/** Static lights array. */
var private const array<SpeedTreeStaticLight> StaticLights;

/** The component's branch light-map. */
var native private const LightMapRef BranchLightMap;

/** The component's frond light-map. */
var native private const LightMapRef FrondLightMap;

/** The component's leaf mesh light-map. */
var native private const LightMapRef LeafMeshLightMap;

/** The component's leaf card light-map. */
var native private const LightMapRef LeafCardLightMap;

/** The component's billboard light-map. */
var native private const LightMapRef BillboardLightMap;

/** The component's rotation matrix (for arbitrary rotations with wind) */
var native private const Matrix RotationOnlyMatrix;

/** The Lightmass settings for the entire speedtree. */
var(Lightmass) LightmassPrimitiveSettings LightmassSettings <ScriptOrder=true>;

/** Returns the component's material corresponding to MeshType if it is set, otherwise returns the USpeedTree's material. */
native function MaterialInterface GetMaterial(ESpeedTreeMeshType MeshType) const;

/** Sets the component's material override, and reattaches if necessary. */
native function SetMaterial(ESpeedTreeMeshType MeshType, MaterialInterface Material);

cpptext
{
	// UPrimitiveComponent interface
#if WITH_SPEEDTREE
	virtual void UpdateBounds();
	FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options);
	
	/**
	* Returns the light and shadow map memory for this primite in its out variables.
	*
	* Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
	*
	* @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
	* @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
	*/
	virtual void GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const;

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;

	/**
	 *	Requests whether the component will use texture, vertex or no lightmaps.
	 *
	 *	@return	ELightMapInteractionType		The type of lightmap interaction the component will use.
	 */
	virtual ELightMapInteractionType GetStaticLightingType() const	{ return LMIT_Vertex;	}
	virtual	void InvalidateLightingCache();
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

	virtual	UBOOL PointCheck(FCheckResult& cResult, const FVector& cLocation, const FVector& cExtent, DWORD dwTraceFlags);
	virtual UBOOL LineCheck(FCheckResult& cResult, const FVector& cEnd, const FVector& cStart, const FVector& cExtent, DWORD dwTraceFlags);
#if WITH_NOVODEX
	virtual void InitComponentRBPhys(UBOOL bFixed);
#endif
#endif

	// UActorComponent interface
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual UBOOL IsValidComponent() const;

	// UComponent interface.
	virtual UBOOL AreNativePropertiesIdenticalTo(UComponent* Other) const;

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostEditUndo();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Default Properties

defaultproperties
{
	bUseLeafCards		= TRUE
	bUseLeafMeshes		= TRUE
	bUseBranches		= TRUE
	bUseFronds			= TRUE
	bUseBillboards		= TRUE
	
	Lod3DStart			= 500.0f
	Lod3DEnd			= 3000.0f
	LodBillboardStart	= 3500.0f
	LodBillboardEnd		= 4000.0f
	LodLevelOverride	= 1.0f
	
	SpeedTreeIcon		= Texture2D'EditorResources.SpeedTreeLogo'

	CollideActors		= TRUE
	BlockActors			= TRUE
	BlockRigidBody		= TRUE
	BlockZeroExtent		= TRUE
	BlockNonZeroExtent	= TRUE

	bUseAsOccluder		= TRUE
	CastShadow			= TRUE
	bAcceptsLights		= TRUE
	bUsePrecomputedShadows = TRUE
}
