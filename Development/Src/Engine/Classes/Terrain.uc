/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Terrain extends Info
	dependson(LightComponent)
	native(Terrain)
	showcategories(Movement,Collision)
	placeable;

// Structs that are mirrored properly in C++.

/**
 *	A height data entry that is stored in an array for the terrain.
 *	Full structure can be found in UnTerrain.h, FTerrainHeight.
 */
struct TerrainHeight
{
	// No UObject reference.
};

/**
 *	InfoData entries for each patch in the terrain.
 *	This includes information such as whether the patch is visible or not (holes).
 *	Full structure can be found in UnTerrain.h, FTerrainInfoData.
 */
struct TerrainInfoData
{
	// No UObject reference.
};

/**
 *	A weighted material used on the terrain.
 *	Full structure can be found in UnTerrain.h, FTerrainWeightedMaterial.
 */
struct TerrainWeightedMaterial
{
	// UObject references.
};

/**
 *	A layer that can be painted onto the terrain.
 */
struct TerrainLayer
{
	/**	The name of the layer, for UI display purposes.										*/
	var() string			Name;
	/**	The TerrainLayerSetup, which declares the material(s) used in the layer.			*/
	var() TerrainLayerSetup	Setup;
	/**	INTERNAL: The index of the alpha map that represents the application of this layer.	*/
	var int					AlphaMapIndex;
	/**	Whether the layer should be highlighted when rendered.								*/
	var() bool				Highlighted;
	/**	Whether the layer should be wireframe highlighted when rendered.
	 *	CURRENTLY NOT IMPLEMENTED
	 */
	var() bool				WireframeHighlighted;
	/**	Whether the layer is hidden (not rendered).											*/
	var() bool				Hidden;
	/**	The color to highlight the layer with.												*/
	var() color				HighlightColor;
	/**	The color to wireframe highlight the layer with.									*/
	var() color				WireframeColor;
	/**
	 *	Rectangle encompassing all the vertices this layer affects.
	 *	TerrainLayerSetup::SetMaterial() uses this to avoid rebuilding
	 *	terrain that has not changed
	 */
	var int MinX, MinY, MaxX, MaxY;

	structdefaultproperties
	{
		AlphaMapIndex=-1
		HighlightColor=(R=255,G=255,B=255)
	}
};

/**
 *	A mapping used to apply a layer to the terrain.
 *	Full structure can be found in UnTerrain.h, FAlphaMap.
 */
struct AlphaMap
{
	// No UObject references.
};

/**
 *	A decoration instance applied to the terrain.
 *	Used internally to apply DecoLayers.
 */
struct TerrainDecorationInstance
{
	var PrimitiveComponent	Component;
	var float				X,
							Y,
							Scale;
	var int					Yaw;
};

/**
 *	A decoration source for terrain DecoLayers.
 */
struct TerrainDecoration
{
	/**	The factory used to generate the decoration mesh.					*/
	var() editinline	PrimitiveComponentFactory	Factory;
	/**	The min scale to apply to the source mesh.							*/
	var() float										MinScale;
	/**	The max scale to apply to the source mesh.							*/
	var() float										MaxScale;
	/**	The density to use when applying the mesh to the terrain.			*/
	var() float										Density;
	/**
	 *	The amount to rotate the mesh to match the slope of the terrain
	 *	where it is being placed. If 1.0, the mesh will match the slope
	 *	exactly.
	 */
	var() float										SlopeRotationBlend;
	/**	The value to use to seed the random number generator.				*/
	var() int										RandSeed;

	/**
	 *	INTERNAL: An array of instances of the decoration applied to the
	 *	terrain.
	 */
	var array<TerrainDecorationInstance>			Instances;

	structdefaultproperties
	{
		Density=0.01
		MinScale=1.0
		MaxScale=1.0
	}
};

/**
 *	A decoration layer - used to easily apply static meshes to the terrain
 */
struct TerrainDecoLayer
{
	/**	The name of the DecoLayer, for UI display purposes.									*/
	var() string					Name;
	/**	The decoration(s) to apply for this layer.											*/
	var() array<TerrainDecoration>	Decorations;
	/**	INTERNAL: The index of the alpha map that represents the application of this layer.	*/
	var int							AlphaMapIndex;

	structdefaultproperties
	{
		AlphaMapIndex=-1
	}
};

/**
 *	Terrain material resource - compiled terrain material used to render the terrain.
 *	Full structure can be found in UnTerrain.h, FTerrainMaterialResource.
 */
struct TerrainMaterialResource
{
	// UObject references.
};

/**	Array of the terrain heights												*/
var private const native array<TerrainHeight>	Heights;
/** Array of the terrain information data (visible, etc.)						*/
var private const native array<TerrainInfoData>	InfoData;
/** Array of the terrain layers applied to the terrain							*/
var() const array<TerrainLayer>					Layers;
/**
 *	The index of the layer that supplies the normal map for the whole terrain.
 *	If this is -1, the terrain will compile the normal property the old way
 *		(all normal maps blended together).
 *	If this is a valid index into the layer array, it will compile the normal
 *		property only for the material(s) contained in said layer.
 */
var() int								NormalMapLayer;
/**	Array of the decoration layers applied										*/
var() const array<TerrainDecoLayer>		DecoLayers;
/**	Array of the alpha maps between layers										*/
var native const array<AlphaMap>		AlphaMaps;

/** The array of terrain components that are used by the terrain				*/
var const NonTransactional array<TerrainComponent>	TerrainComponents;

/**
 *	Internal values used to setup components
 *
 *	The number of sections is the number of terrain components along the
 *	X and Y of the 'grid'.
 */
var const int							NumSectionsX;
var const int							NumSectionsY;

/**	INTERNAL - The weighted materials and blend maps							*/
var private native const array<TerrainWeightedMaterial>	WeightedMaterials;
var private const native array<TerrainWeightMapTexture>	WeightedTextureMaps;

/**
 *	The maximum number of quads in a single row/column of a tessellated patch.
 *  Must be a power of two, 1 <= MaxTesselationLevel <= 16
 */
var() int						MaxTesselationLevel;

/**
 *	The minimum number of quads in a tessellated patch.
 *	Must be a power of two, 1 <= MaxTesselationLevel
 */
var() int						MinTessellationLevel;

/**
 *	The scale factor to apply to the distance used in determining the tessellation
 *	level to utilize when rendering a patch.
 *		TessellationLevel = SomeFunction((Patch distance to camera) * TesselationDistanceScale)
 */
var() float						TesselationDistanceScale;

/**
 *	The radius from the view origin that terrain tessellation checks should be performed.
 *	If less than 0, the general setting from the engine configuration will be used.
 *	If 0.0, every component will be checked for tessellation changes each frame.
 */
var() float						TessellationCheckDistance;

/**
 *	The tessellation level to utilize when performing collision checks with non-zero extents.
 */
var(Collision) int				CollisionTesselationLevel;

struct native CachedTerrainMaterialArray
{
	var native const array<pointer> CachedMaterials{FTerrainMaterialResource};
};
/** 
 * array of cached terrain materials 
 * Only the first entry is ever used now that SM2 is no longer supported, 
 * But the member is kept as an array to make adding future material platforms easier.  
 * The second entry is to work around the script compile error from having an array with one element.
 */
var native const CachedTerrainMaterialArray CachedTerrainMaterials[2];

/**
 * The number of vertices currently stored in a single row of height and alpha data.
 * Updated from NumPatchesX when Allocate is called(usually from PostEditChange).
 */
var const int					NumVerticesX;

/**
 * The number of vertices currently stored in a single column of height and alpha data.
 * Updated from NumPatchesY when Allocate is called(usually from PostEditChange).
 */
var const int					NumVerticesY;

/**
 *  The number of patches in a single row of the terrain's patch grid.
 *  PostEditChange clamps this to be >= 1.
 *	Note that if you make this and/or NumPatchesY smaller, it will destroy the height-map/alpha-map
 *	data which is no longer used by the patches.If you make the dimensions larger, it simply fills
 *	in the new height-map/alpha-map data with zero.
 */
var() int						NumPatchesX;

/**
 *	The number of patches in a single column of the terrain's patch grid.
 *  PostEditChange clamps this to be >= 1.
 */
var() int						NumPatchesY;

/**
 *	For rendering and collision, split the terrain into components with a maximum size of
 *		(MaxComponentSize,MaxComponentSize) patches.
 *	The terrain is split up into rectangular groups of patches called terrain components for rendering.
 *	MaxComponentSize is the maximum number of patches in a single row/column of a terrain component.
 *	Generally, all components will be MaxComponentSize patches square, but on terrains with a patch
 *	resolution which isn't a multiple of MaxComponentSize, there will be some components along the edges
 *	which are smaller.
 *
 *	This is limited by the MaxTesselationLevel, to prevent the vertex buffer for a fully tessellated
 *	component from being > 65536 vertices.
 *	For a MaxTesselationLevel of 16, MaxComponentSize is limited to <= 15.
 *	For a MaxTesselationLevel of 8, MaxComponentSize is limited to <= 31.
 *
 *	PostEditChange clamps this to be >= 1.
 */
var() int						MaxComponentSize;

/**
 *	The resolution to cache lighting at, in texels/patch.
 *	A separate shadow-map is used for each terrain component, which is up to
 *	(MaxComponentSize * StaticLightingResolution + 1) pixels on a side.
 *	Must be a power of two, 1 <= StaticLightingResolution <= MaxTesselationLevel.
 */
var(Lighting) int				StaticLightingResolution;

/**
 *	If true, the light/shadow map size is no longer restricted...
 *	The size of the light map will be (per component):
 *		INT LightMapSizeX = Component->SectionSizeX * StaticLightingResolution + 1;
 *		INT LightMapSizeY = Component->SectionSizeY * StaticLightingResolution + 1;
 *
 *	So, the maximum size of a light/shadow map for a component will be:
 *		MaxMapSizeX = MaxComponentSize * StaticLightingResolution + 1
 *		MaxMapSizeY = MaxComponentSize * StaticLightingResolution + 1
 *
 *	Be careful with the setting of StaticLightingResolution when this mode is enabled.
 *	It will be quite easy to run up a massive texture requirement on terrain!
 */
var(Lighting) bool						bIsOverridingLightResolution;

/**
 *	If true, the lightmap generation will be performed using the bilinear filtering
 *	that all other lightmap generation in the engine uses.
 *
 */
var(Lighting) bool						bBilinearFilterLightmapGeneration;

/**
 * Whether terrain should cast shadows.
 *
 * Property is propagated to terrain components
 */
var(Lighting) bool						bCastShadow;

/**
 * If true, forces all static lights to use light-maps for direct lighting on the terrain, regardless of
 * the light's UseDirectLightMap property.
 *
 * Property is propagated to terrain components .
 */
var(Lighting) const bool				bForceDirectLightMap;

/**
 * If false, primitive does not cast dynamic shadows.
 *
 * Property is propagated to terrain components .
 */
var(Lighting) const bool				bCastDynamicShadow;

/**
 *	If TRUE, enable specular on this terrain.
 */
var(Lighting) bool						bEnableSpecular;

/**
 * If false, primitive does not block rigid body physics.
 *
 * Property is propagated to terrain components.
 */
var(Collision) const bool				bBlockRigidBody;

/** If true, this allows rigid bodies to go underneath visible areas of the terrain. This adds some physics cost. */
var(Collision) const bool				bAllowRigidBodyUnderneath;

/** PhysicalMaterial to use for entire terrain */
var(Physics) const PhysicalMaterial	TerrainPhysMaterialOverride;

/**
 * If false, primitive does not accept dynamic lights, aka lights with HasStaticShadowing() == FALSE
 *
 * Property is propagated to terrain components.
 */
var(Lighting) const bool				bAcceptsDynamicLights;

/**
 * Lighting channels controlling light/ primitive interaction. Only allows interaction if at least one channel is shared */
var(Lighting) const LightingChannelContainer	LightingChannels;

/**
 *	Lightmass settings for the terrain
 */
var(Lightmass) LightmassPrimitiveSettings	LightmassSettings <ScriptOrder=true>;

/**
 *	Whether to utilize morping terrain or not
 */
var()		bool				bMorphingEnabled;
/**
 *	Whether to utilize morping gradients or not (bMorphingEnabled must be true for this to matter)
 */
var()		bool				bMorphingGradientsEnabled;

/**	The terrain is locked - no editing can take place on it	*/
var			bool				bLocked;

/**	The terrain heightmap is locked - no editing can take place on it	*/
var			bool				bHeightmapLocked;

/** Command fence used to shut down properly */
var native const pointer		ReleaseResourcesFence{FRenderCommandFence};

/** Editor-viewing tessellation level						*/
var() transient	int				EditorTessellationLevel;

/** Viewing collision tessellation level					*/
var			bool				bShowingCollision;

/** Base UVs from world origin, to avoid layering seams in adjacent terrains */
var()		bool				bUseWorldOriginTextureUVs;

/** Selected vertex structure - used for vertex editing		*/
struct SelectedTerrainVertex
{
	/** The position of the vertex.					*/
	var int		X, Y;
	/** The weight of the selection.				*/
	var int		Weight;
};

var transient	array<SelectedTerrainVertex>	SelectedVertices;

/** Tells the terrain to render in wireframe. */
var() 			bool				bShowWireframe;
/** The color to use when rendering the wireframe of the terrain. */
var() 			color				WireframeColor;

/** Unique ID for this terrain, used for caching during distributed lighting */
var private const editoronly Guid LightingGuid;

cpptext
{
    // UObject interface

	virtual void Serialize(FArchive& Ar);
	virtual void PreEditChange(UProperty* PropertyThatChanged);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostEditMove(UBOOL bFinished);

protected:
	void HandleLegacyTextureReferences();
public:

	virtual void PostLoad();

	/**
	 * @return		Sum of the size of textures referenced by this material.
	 */
	virtual INT GetResourceSize();

	/**
	 *	Called before the Actor is saved.
	 */
	virtual void PreSave();
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();

	virtual void ClearWeightMaps();
	virtual void TouchWeightMapResources();

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
	 */
	void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	// AActor interface

	virtual void Spawned();
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);

	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid; 
#endif // WITH_EDITORONLY_DATA
	}

	virtual void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = appCreateGuid();
#endif // WITH_EDITORONLY_DATA
	}

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();

	/**
	 * Function that is called from CheckForErrors - specifically checks for material errors.
	 */
	void CheckForMaterialErrors();
#endif
	virtual void ClearComponents();

	/** Called by the lighting system to allow actors to order their components for deterministic lighting */
	virtual void OrderComponentsForDeterministicLighting();

protected:
	virtual void UpdateComponentsInternal(UBOOL bCollisionUpdate = FALSE);
public:
	virtual void UpdatePatchBounds(INT MinX,INT MinY,INT MaxX,INT MaxY);

	void WeldEdgesToOtherTerrains();

	virtual UBOOL ActorLineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	// CompactAlphaMaps - Cleans up alpha maps that are no longer used.

	void CompactAlphaMaps();

	// CacheWeightMaps - Generates the weightmaps from the layer stack and filtered materials.

	void CacheWeightMaps(INT MinX,INT MinY,INT MaxX,INT MaxY);

	// CacheDecorations - Generates a set of decoration components for an area of the terrain.
	void CacheDecorations(INT MinX,INT MinY,INT MaxX,INT MaxY);

	// UpdateRenderData - Updates the weightmaps, displacements, decorations, vertex buffers and bounds when the heightmap, an alphamap or a terrain property changes.
	void UpdateRenderData(INT MinX,INT MinY,INT MaxX,INT MaxY);

	/** updates decoration components to account for terrain/layer property changes */
	void UpdateDecorationComponents();

	/** Clamps the vertex index to a valid vertex index (0 to NumVerticesX - 1, 0 to NumVerticesY - 1) that can be used to address the vertex collection.
	 * An invalid vertex index is something like (-1,-1) which would cause an array out of bounds exception.
	 *
	 * @param	OutX	The clamped X coordinate.
	 * @param	OutY	The clamped Y coordinate.
	 */
	void ClampVertexIndex(INT& OutX, INT& OutY) const;

	/**
	 * Allocates and initializes resolution dependent persistent data. (height-map, alpha-map, components)
	 * Keeps the old height-map and alpha-map data, cropping and extending as necessary.
	 * Uses DesiredSizeX, DesiredSizeY to determine the desired resolution.
	 * DesiredSectionSize determines the size of the components the terrain is split into.
	 */
	void Allocate();

	/**
	  * Recreates all the components
	  */
	void RecreateComponents();

	/**
	 *	Split a terrain along the X or Y axis
	 *	Returns the new terrain if successful
	 */
	ATerrain* SplitTerrain( UBOOL SplitOnXAxis, INT RemainingPatches );
	void SplitTerrainPreview( class FPrimitiveDrawInterface* PDI, UBOOL SplitOnXAxis, INT RemainingPatches );

	/**
	 *	Merges this terrain with another specified terrain if possible
	 *	Returns success TRUE/FALSE
	 */
	UBOOL MergeTerrain( ATerrain* Other );
	UBOOL MergeTerrainPreview( class FPrimitiveDrawInterface* PDI, ATerrain* Other );

	/**
	 *	Add or remove sectors to the terrain
	 *
	 *	@param	CountX		The number of sectors in the X-direction. If negative,
	 *						they will go to the left, otherwise to the right.
	 *	@param	CountY		The number of sectors in the Y-direction. If negative,
	 *						they will go to the bottom, otherwise to the top.
	 *	@param	bRemove		If TRUE, remove the sectors, otherwise add them.
	 *
	 *	@return	UBOOL		TRUE if successful.
	 */
	UBOOL AddRemoveSectors(INT CountX, INT CountY, UBOOL bRemove);

	// Internal functions for adding/removing sectos
	void StoreOldData(TArray<FTerrainHeight>& OldHeights, TArray<FTerrainInfoData>& OldInfoData, TArray<FAlphaMap>& OldAlphaMaps);
	void SetupSizeData();
	UBOOL AddSectors_X(INT Count);
	UBOOL AddSectors_Y(INT Count);
	UBOOL RemoveSectors_X(INT Count);
	UBOOL RemoveSectors_Y(INT Count);

	// Data access.

	//
	//	ATerrain::Height
	//
	inline const WORD& Height(INT X,INT Y) const
	{
		X = Clamp(X,0,NumVerticesX - 1);
		Y = Clamp(Y,0,NumVerticesY - 1);
		return Heights(Y * NumVerticesX + X).Value;
	}

	//
	//	ATerrain::Height
	//
	inline WORD& Height(INT X,INT Y)
	{
		X = Clamp(X,0,NumVerticesX - 1);
		Y = Clamp(Y,0,NumVerticesY - 1);
		return Heights(Y * NumVerticesX + X).Value;
	}

	inline WORD BilinearHeight(FLOAT fX,FLOAT fY)
	{
		INT X = appFloor(fX);
		INT Y = appFloor(fY);
		FLOAT tx = fX - (FLOAT)X;
		FLOAT ty = fY - (FLOAT)Y;

		return appRound(
				Lerp(
				Lerp( (FLOAT)Height(X,Y), (FLOAT)Height(X+1,Y), tx),
				Lerp( (FLOAT)Height(X,Y+1), (FLOAT)Height(X+1,Y+1), tx),
				ty) );
	}

	inline FTerrainInfoData* GetInfoData(INT X, INT Y)
	{
		X = Clamp(X, 0, NumVerticesX - 1);
		Y = Clamp(Y, 0, NumVerticesY - 1);

		return &(InfoData(Y * NumVerticesX + X));
	}

	inline const FTerrainInfoData* GetInfoData(INT X, INT Y) const
	{
		X = Clamp(X, 0, NumVerticesX - 1);
		Y = Clamp(Y, 0, NumVerticesY - 1);

		return &(InfoData(Y * NumVerticesX + X));
	}

	inline UBOOL IsTerrainQuadVisible(INT X, INT Y) const
	{
		const FTerrainInfoData* TheInfoData = GetInfoData(X, Y);
		checkSlow(TheInfoData);
		return TheInfoData->IsVisible();
	}

	inline UBOOL IsTerrainQuadFlipped(INT X, INT Y) const
	{
		const FTerrainInfoData* TheInfoData = GetInfoData(X, Y);
		checkSlow(TheInfoData);
		return TheInfoData->IsOrientationFlipped();
	}

	/**
	 *	Returns TRUE is the component at the given X,Y has ANY patches contained in are visible.
	 */
	UBOOL IsTerrainComponentVisible(INT InBaseX, INT InBaseY, INT InSizeX, INT InSizeY);
	UBOOL IsTerrainComponentVisible(UTerrainComponent* InComponent);

	FVector GetLocalVertex(INT X,INT Y) const; // Returns a vertex in actor-local space.
	FVector GetWorldVertex(INT X,INT Y) const; // Returns a vertex in world space.

	FTerrainPatch GetPatch(INT X,INT Y) const;
	FVector GetCollisionVertex(const FTerrainPatch& Patch,UINT PatchX,UINT PatchY,UINT SubX,UINT SubY,UINT TesselationLevel) const;

	BYTE Alpha(INT AlphaMapIndex,INT X,INT Y) const;		// If AlphaMapIndex == INDEX_NONE, returns 0.
	BYTE& Alpha(INT& AlphaMapIndex,INT X,INT Y);			// If AlphaMapIndex == INDEX_NONE, creates a new alphamap and places the index in AlphaMapIndex.

	/**
	* Returns a cached terrain material containing a given set of weighted materials.
	* Generates a new entry if not found
	*
	* @param Mask - bitmask combination of weight materials to be used
	* @param bIsTerrainResource - [out] TRUE if the material resource returned is a terrain material, FALSE if fallback
	* @return terrain material resource render proxy or error material render proxy
	*/
	FMaterialRenderProxy* GetCachedMaterial(const FTerrainMaterialMask& Mask, UBOOL& bIsTerrainResource);

	/**
	* Creates new cached terrain material entry if it doesn't exist for the given mask
	*
	* @param Mask - bitmask combination of weight materials to be used
	* @return new terrain material resource
	*/
	FTerrainMaterialResource* GenerateCachedMaterial(const FTerrainMaterialMask& Mask);

	/**
	 *	RetrieveReleaseResourcesFence
	 *
	 *	This function will grab the ReleaseResourcesFence.
	 *	If it is NULL, it will create one.
	 *	Should be used when a fence is required.
	 *
	 *	@return FRenderCommandFence		The ReleaseResourcesFence returned.
	 */
	FRenderCommandFence* RetrieveReleaseResourcesFence()
	{
		if (ReleaseResourcesFence == NULL)
		{
			ReleaseResourcesFence = ::new FRenderCommandFence();
			check(ReleaseResourcesFence);
		}
		return ReleaseResourcesFence;
	}

	/**
	 *	GetReleaseResourcesFence
	 *
	 *	This function will grab the current ReleaseResourcesFence.
	 *	Should be used when a fence should be checked if it has been created previously.
	 *
	 *	@return FRenderCommandFence		The ReleaseResourcesFence returned.
	 */
	FRenderCommandFence* GetReleaseResourcesFence()
	{
		return ReleaseResourcesFence;
	}

	/**
	 *	FreeReleaseResourcesFence
	 *
	 *	This function will free the current ReleaseResourcesFence.
	 *	Should be used when a fence should be checked if it has been created previously.
	 *
	 *	@return FRenderCommandFence		The ReleaseResourcesFence returned.
	 */
	void FreeReleaseResourcesFence()
	{
		if (ReleaseResourcesFence != NULL)
		{
			delete ReleaseResourcesFence;
			ReleaseResourcesFence = NULL;
		}
	}

	/**
	 *	MaterialUpdateCallback
	 *
	 *	Called when materials are edited to propagate the change to terrain materials.
	 *
	 *	@param	InMaterial		The material that was edited.
	 *
	 */
	static void MaterialUpdateCallback(UMaterial* InMaterial);

	/**
	 *	BuildCollisionData
	 *
	 *	Helper function to force the re-building of the collision date.
	 */
	void BuildCollisionData();

	/**
	 *	RecacheMaterials
	 *
	 *	Helper function that tosses the cached materials and regenerates them.
	 */
	void RecacheMaterials();

	/**
	 *	UpdateLayerSetup
	 *
	 *	Editor function for updating altered materials/layers
	 *
	 *	@param	InSetup		The layer setup to update.
	 */
	void UpdateLayerSetup(UTerrainLayerSetup* InSetup);

	/**
	 *	RemoveLayerSetup
	 *
	 *	Editor function for removing altered materials/layers
	 *
	 *	@param	InSetup		The layer setup to remove.
	 */
	void RemoveLayerSetup(UTerrainLayerSetup* InSetup);

	/**
	 *	UpdateTerrainMaterial
	 *
	 *	Editor function for updating altered materials/layers
	 *
	 *	@param	InTMat		The terrain material to update.
	 */
	void UpdateTerrainMaterial(UTerrainMaterial* InTMat);

	/**
	 *	RemoveTerrainMaterial
	 *
	 *	Editor function for removing altered materials/layers
	 *
	 *	@param	InTMat		The terrain material to Remove.
	 */
	void RemoveTerrainMaterial(UTerrainMaterial* InTMat);

	/**
	 *	UpdateMaterialInstance
	 *
	 *	Editor function for updating altered materials/layers
	 *
	 *	@param	InMatInst	The material instance to update.
	 */
	void UpdateMaterialInstance(UMaterialInterface* InMatInst);

	/**
	 *	UpdateCachedMaterial
	 *
	 *	Editor function for updating altered materials/layers
	 *
	 *	@param	InMat		The material instance to update.
	 */
	void UpdateCachedMaterial(UMaterial* InMat);

	/**
	 *	RemoveCachedMaterial
	 *
	 *	Editor function for removing altered materials/layers
	 *
	 *	@param	InMat		The material instance to remove.
	 */
	void RemoveCachedMaterial(UMaterial* InMat);

	/**
	 *	Tessellate the terrain up in detail
	 *	Also used for converting old terrain to the new hi-res model
	 *
	 *	REQUIRES UPDATING TERRAIN MATERIAL MAPPING SCALES BY HAND!
	 *
	 *	@param	InTessellationlevel		The tessellation level to increase it to
	 *
	 *	@return	UBOOL					TRUE if successful
	 */
	UBOOL TessellateTerrainUp(INT InTessellationlevel = 2, UBOOL bRegenerateComponents = TRUE);

	/**
	 *	Tessellate the terrain down in detail.
	 *	Will remove patches while retaining the 'shape' of the terrain.
	 *
	 *	REQUIRES UPDATING TERRAIN MATERIAL MAPPING SCALES BY HAND!
	 */
	UBOOL TessellateTerrainDown();

	/**
	 *	GetClosestVertex
	 *
	 *	Determine the vertex that is closest to the given location.
	 *	Used for drawing tool items.
	 *
	 *	@param	InLocation		FVector representing the location caller is interested in
	 *	@param	OutVertex		FVector the function will fill in
	 *	@param	bConstrained	If TRUE, then select the closest according to editor tessellation level
	 *
	 *	@return	UBOOL			TRUE indicates the point was found and OutVertex is valid.
	 *							FALSE indicates the point was not contained within the terrain.
	 */
	UBOOL GetClosestVertex(const FVector& InLocation, FVector& OutVertex, UBOOL bConstrained = FALSE);

	/**
	 *	GetClosestLocalSpaceVertex
	 *
	 *	Determine the vertex that is closest to the given location in local space.
	 *	The returned position is also in local space.
	 *	Used for drawing tool items.
	 *
	 *	@param	InLocation		FVector representing the location caller is interested in
	 *	@param	OutVertex		FVector the function will fill in
	 *	@param	bConstrained	If TRUE, then select the closest according to editor tessellation level
	 *
	 *	@return	UBOOL			TRUE indicates the point was found and OutVertex is valid.
	 *							FALSE indicates the point was not contained within the terrain.
	 */
	UBOOL GetClosestLocalSpaceVertex(const FVector& InLocation, FVector& OutVertex, UBOOL bConstrained = FALSE);

	/**
	 *	ShowCollisionCallback
	 *
	 *	Called when SHOW terrain collision is toggled.
	 *
	 *	@param	bShow		Whether to show it or not.
	 *
	 */
	static void ShowCollisionCallback(UBOOL bShow);

	/**
	 *	Show/Hide terrain collision overlay
	 *
	 *	@param	bShow				Show or hide
	 */
	void ShowCollisionOverlay(UBOOL bShow);

	/**
	 *	Update the given selected vertex in the list.
	 *	If the vertex is not present, then add it to the list (provided Weight > 0)
	 *
	 *	@param	X
	 *	@param	Y
	 *	@param	Weight
	 *
	 */
	void UpdateSelectedVertex(INT X, INT Y, FLOAT Weight);

	/**
	 *	Internal function for getting a selected vertex from the list
	 */
	INT FindSelectedVertexInList(INT X, INT Y, FSelectedTerrainVertex*& SelectedVert);

	/**
	 *	Clear all selected vertices
	 */
	void ClearSelectedVertexList();

	/**
	 *	Retrieve the component(s) that contain the given vertex point
	 *	The components will be added (using AddUniqueItem) to the supplied array.
	 *
	 *	@param	X				The X position of interest
	 *	@param	Y				The Y position of interest
	 *	@param	ComponentList	The array to add found components to
	 *
	 *	@return	UBOOL			TRUE if any components were found.
	 *							FALSE if none were found
	 */
	UBOOL GetComponentsAtXY(INT X, INT Y, TArray<UTerrainComponent*>& ComponentList);

	/**
	 *	Recache the visibility flags - used when changing tessellation levels.
	 */
	void RecacheVisibilityFlags();

	/**
	 * Get the array of cached terrain material resource 
	 */
	FORCEINLINE TArrayNoInit<FTerrainMaterialResource*>& GetCachedTerrainMaterials()
	{
		return CachedTerrainMaterials[MSQ_TERRAIN].CachedMaterials;
	}

	/**
	 * Delete the entries in the cached terrain materials
	 */
	void ClearCachedTerrainMaterials();

	/**
	* Compiles material resources for the current platform if the shader map for that resource didn't already exist.
	*
	* @param ShaderPlatform - platform to compile for
	* @param bFlushExistingShaderMaps - forces a compile, removes existing shader maps from shader cache.
	*/
	void CacheResourceShaders(EShaderPlatform ShaderPlatform, UBOOL bFlushExistingShaderMaps=FALSE);
}

/** for each layer, calculate the rectangle encompassing all the vertices affected by it and store the result in
 * the layer's MinX, MinY, MaxX, and MaxY properties
 */
native final function CalcLayerBounds();

simulated event PostBeginPlay()
{
	local int i;

	CalcLayerBounds();

	// allow any layers to run startup actions
	for (i = 0; i < Layers.length; i++)
	{
		if (Layers[i].Setup != None)
		{
			Layers[i].Setup.PostBeginPlay();
		}
	}
}

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.S_Terrain'
	End Object

	NormalMapLayer=-1
	NumPatchesX=1
	NumPatchesY=1
	MaxComponentSize=16

	DrawScale3D=(X=256.0,Y=256.0,Z=256.0)
	bEdShouldSnap=True
	bCollideActors=True
	bBlockActors=True
	bWorldGeometry=True
	bStatic=True
	bNoDelete=True
	bHidden=False
	MaxTesselationLevel=4
	MinTessellationLevel=1
	CollisionTesselationLevel=1
	TessellationCheckDistance=-1.0
	TesselationDistanceScale=1.0
	StaticLightingResolution=4
	bIsOverridingLightResolution=false
	bBilinearFilterLightmapGeneration=true
	bCastShadow=True
	bCastDynamicShadow=True
	bBlockRigidBody=True
	bAcceptsDynamicLights=True
	LightingChannels=(Static=TRUE,bInitialized=TRUE)
	bForceDirectLightMap=TRUE
	WireframeColor=(R=0,G=255,B=255)
}
