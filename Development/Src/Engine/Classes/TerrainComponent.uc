/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TerrainComponent extends PrimitiveComponent
	native(Terrain);

/**	INTERNAL: Array of shadow map data applied to the terrain component.		*/
var private const array<ShadowMap2D> ShadowMaps;
/**	INTERNAL: Array of lights that don't apply to the terrain component.		*/
var const array<Guid>				IrrelevantLights;

var const native transient pointer	TerrainObject{struct FTerrainObject};
var const int						SectionBaseX,
									SectionBaseY,
									SectionSizeX,
									SectionSizeY;
									
/** The actual section size in vertices...										*/
var const int						TrueSectionSizeX;
var const int						TrueSectionSizeY;

var native private const LightMapRef LightMap;

struct TerrainPatchBounds
{
	var float MinHeight;
	var float MaxHeight;
	var float MaxDisplacement;
};

struct TerrainMaterialMask
{
	var qword BitMask;
	var int NumBits;
};

var private const native transient array<TerrainPatchBounds>	PatchBounds;
var private const native transient array<TerrainMaterialMask>	BatchMaterials;
var private const native transient int		FullBatch;


/** Place holder structure that mirrors the byte size needed for a BV tree. */
struct TerrainBVTree
{
	var private const native array<int> Nodes;
};

/** Used for in-game collision tests against terrain. */
var private const native transient TerrainBVTree BVTree;

/**
 * This is a low poly version of the terrain vertices in world space. The
 * triangle data is created based upon Terrain->CollisionTesselationLevel
 */
var private const native transient array<vector>		CollisionVertices;

/** Physics engine version of heightfield data. */
var const native pointer RBHeightfield{class NxHeightField};

/**
 *	Indicates the the terrain collision level should be rendered.
 */
var private const bool	bDisplayCollisionLevel;


cpptext
{
	/**
	 * Builds the collision data for this terrain
	 */
	void BuildCollisionData(void);

	/**
	 * @return Whether or not the collision data for this component is dirty.
	 */
	UBOOL IsCollisionDataDirty() const
	{
		// @todo: Replace this with a proper flag, this is only a stub function right now.

		return TRUE;
	}

	// UObject interface.

	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );
	virtual void Serialize( FArchive& Ar );
	virtual void PostLoad();
	virtual void FinishDestroy();
	/**
	* @return		Sum of the size of textures referenced by this material.
	*/
	virtual INT GetResourceSize();

	/**
	 * Rebuilds the collision data for saving
	 */
	virtual void PreSave(void);

	// UPrimitiveComponent interface.

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);
	virtual void UpdateBounds();
	
	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;

	virtual void InitComponentRBPhys(UBOOL bFixed);

	/**
	 * Returns the MAX number of triangle this component will render.
	 *
	 *	@return	UINT		Maximum number of triangle that could be rendered.
	 */
	virtual UINT GetMaxTriangleCount( ) const;

	/**
	 * Returns the lightmap resolution used for this primivite instnace in the case of it supporting texture light/ shadow maps.
	 * 0 if not supported or no static shadowing.
	 *
	 * @param	Width	[out]	Width of light/shadow map
	 * @param	Height	[out]	Height of light/shadow map
	 *
	 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
	 */
	virtual UBOOL GetLightMapResolution( INT& Width, INT& Height ) const;

	/**
	 *	Returns the static lightmap resolution used for this primitive.
	 *	0 if not supported or no static shadowing.
	 *
	 * @return	INT		The StaticLightmapResolution for the component
	 */
	virtual INT GetStaticLightMapResolution() const;

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
	 *  Retrieve various actor metrics depending on the provided type.  All of
	 *  these will total the values for this component.
	 *
	 *  @param MetricsType The type of metric to calculate.
	 *
	 *  METRICS_VERTS    - Get the number of vertices.
	 *  METRICS_TRIS     - Get the number of triangles.
	 *  METRICS_SECTIONS - Get the number of sections.
	 *
	 *  @return INT The total of the given type for this component.
	 */
	virtual INT GetActorMetrics(EActorMetricsType MetricsType);

	friend struct FTerrainObject;
	friend class FTerrainComponentSceneProxy;

	// UActorComponent interface.
protected:
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );

	/**
	* Only valid for cases when the primitive will be reattached
	* @return	TRUE if the base primitive component should handle reattaching decals when the primitive is attached
	*/
	virtual UBOOL AllowDecalAutomaticReAttach() const
	{
		// always detach decals for terrain since we trigger a manual reattach
		return FALSE;
	}

public:
	virtual void InvalidateLightingCache();

	// UPrimitiveComponent interface.
	virtual void GenerateDecalRenderData(class FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const;
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options);
	/**
	 *	Requests whether the component will use texture, vertex or no lightmaps.
	 *
	 *	@return	ELightMapInteractionType		The type of lightmap interaction the component will use.
	 */
	virtual ELightMapInteractionType GetStaticLightingType() const	{ return LMIT_Texture;	}
	virtual void GetStaticTriangles(FPrimitiveTriangleDefinitionInterface* PTDI) const;
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

	// Init

	void Init(INT InBaseX,INT InBaseY,INT InSizeX,INT InSizeY,INT InTrueSizeX,INT InTrueSizeY);

	// UpdatePatchBounds

	void UpdatePatchBounds();

	/** builds/updates a list of unique blended material combinations used by quads in this terrain section and puts them in the PatchBatches array.
	 * Also updates FullBatch with the index of the mask for the full set.
	 */
	void UpdatePatchBatches();

	/** Return's the terrain actor associated with the terrain component. */
	class ATerrain* GetTerrain() const
	{
		return CastChecked<ATerrain>(GetOuter());
	}

	/** Returns a vertex in the component's local space. */
	FVector GetLocalVertex(INT X,INT Y) const;

	/** Returns a vertex in the component's local space. */
	FVector GetWorldVertex(INT X,INT Y) const;

	// 
	void RenderPatches(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	/**
	 * Gets the terrain collision data needed to pass to Novodex or to the
	 * kDOP code. Note: this code generates vertices/indices based on the
	 * Terrain->CollisionTessellationLevel
	 *
	 * @param OutVertices		[out] The array that gets each vert in the terrain
	 * @param OutIndices		[out] The array that holds the generated indices
	 */
	void GetCollisionData(TArray<FVector>& OutVertices,TArray<INT>& OutIndices) const;

	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	UINT GetTriangleCount();
	UINT GetTriangleCountForDecal( UDecalComponent * DecalComponent );
}


defaultproperties
{
	CollideActors=TRUE
	BlockActors=TRUE
	BlockZeroExtent=TRUE
	BlockNonZeroExtent=TRUE
	BlockRigidBody=TRUE
	CastShadow=TRUE
	bAcceptsLights=TRUE
	bAcceptsDecals=TRUE
	bAcceptsStaticDecals=TRUE
	bUsePrecomputedShadows=TRUE
	bUseAsOccluder=TRUE
	bAllowCullDistanceVolume=FALSE
}
