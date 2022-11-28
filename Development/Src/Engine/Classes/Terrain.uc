class Terrain extends Info
	native(Terrain)
	placeable;

// Structs that are mirrored properly in C++.

struct TerrainHeight
{
};

struct TerrainWeightedMaterial
{
};

struct TerrainLayer
{
	var() string			Name;
	var() TerrainLayerSetup	Setup;
	var int					AlphaMapIndex;
	var() bool				Highlighted,
							Hidden;

	structdefaults
	{
		AlphaMapIndex=-1
	};
};

struct AlphaMap
{
};

struct TerrainDecorationInstance
{
	var PrimitiveComponent	Component;
	var float				X,
							Y,
							Scale;
	var int					Yaw;
};

struct TerrainDecoration
{
	var() editinlinenotify PrimitiveComponentFactory	Factory;
	var() float											MinScale;
	var() float											MaxScale;
	var() float											Density;
	var() float											SlopeRotationBlend;
	var() int											RandSeed;

	var array<TerrainDecorationInstance>	Instances;

	structdefaults
	{
		Density=0.01
		MinScale=1.0
		MaxScale=1.0
	};
};

struct TerrainDecoLayer
{
	var() string					Name;
	var() array<TerrainDecoration>	Decorations;
	var int							AlphaMapIndex;
	
	structdefaults
	{
		AlphaMapIndex=-1
	};
};

struct TerrainMaterialResource
{
};

struct TerrainMaterialInstancePointer
{
	var native pointer	P;
};

var private const native array<TerrainHeight>	Heights;
var() const array<TerrainLayer>					Layers;
var() const array<TerrainDecoLayer>				DecoLayers;
var native const array<AlphaMap>				AlphaMaps;

var const NonTransactional array<TerrainComponent>	TerrainComponents;
var const int						NumSectionsX,
									NumSectionsY,
									SectionSize; // Legacy!

var private native const array<TerrainWeightedMaterial>	WeightedMaterials;

var native const array<byte>	CachedDisplacements;
var native const float			MaxCollisionDisplacement;

var() int					MaxTesselationLevel;
var() float					TesselationDistanceScale;

var native const array<TerrainMaterialResource>	CachedMaterials;	// C++ declaration in cpptext block below.
var native const TerrainMaterialInstancePointer	MaterialInstance;

/**
 * The number of vertices currently stored in a single row of height and alpha data.
 * Updated from NumPatchesX when Allocate is called(usually from PostEditChange).
 */
var const int NumVerticesX;

/**
 * The number of vertices currently stored in a single column of height and alpha data.
 * Updated from NumPatchesY when Allocate is called(usually from PostEditChange).
 */
var const int NumVerticesY;

/** The number of patches in a single row of the terrain's patch grid.  PostEditChange clamps this to be >= 1. */
var() int NumPatchesX;

/** The number of patches in a single column of the terrain's patch grid.  PostEditChange clamps this to be >= 1. */
var() int NumPatchesY;

/**
 * For rendering and collision, split the terrain into components with a maximum size of (MaxComponentSize,MaxComponentSize) patches.
 * PostEditChange clamps this to be >= 1.
 */
var() int MaxComponentSize;

/**
 * The resolution to cache lighting at, in texels/patch.
 */
var() int StaticLightingResolution;

cpptext
{
    // UObject interface

	virtual void Serialize(FArchive& Ar);
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void PostLoad();
	virtual void Destroy();

	// AActor interface

	virtual void Spawned();
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);

	virtual void InitRBPhys();
	virtual void TermRBPhys();

	virtual void ClearComponents();
	virtual void UpdateComponents();

	virtual void CacheLighting();
	virtual void InvalidateLightingCache();

	virtual UBOOL ActorLineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	// CompactAlphaMaps - Cleans up alpha maps that are no longer used.
	
	void CompactAlphaMaps();

	// CacheWeightMaps - Generates the weightmaps from the layer stack and filtered materials.

	void CacheWeightMaps(INT MinX,INT MinY,INT MaxX,INT MaxY);

	// CacheDisplacements - Caches the amount each tesselated vertex is displaced.

	void CacheDisplacements(INT MinX,INT MinY,INT MaxX,INT MaxY);

	// CacheDecorations - Generates a set of decoration components for an area of the terrain.
	
	void CacheDecorations(INT MinX,INT MinY,INT MaxX,INT MaxY);

	// UpdateRenderData - Updates the weightmaps, displacements, decorations, vertex buffers and bounds when the heightmap, an alphamap or a terrain property changes.

	void UpdateRenderData(INT MinX,INT MinY,INT MaxX,INT MaxY);

	/**
	 * Allocates and initializes resolution dependent persistent data. (height-map, alpha-map, components)
	 * Keeps the old height-map and alpha-map data, cropping and extending as necessary.
	 * Uses DesiredSizeX, DesiredSizeY to determine the desired resolution.
	 * DesiredSectionSize determines the size of the components the terrain is split into.
	 */
	void Allocate();

	// Data access.

	const _WORD& Height(INT X,INT Y) const;
	_WORD& Height(INT X,INT Y);
	FVector GetLocalVertex(INT X,INT Y) const; // Returns a vertex in actor-local space.
	FVector GetWorldVertex(INT X,INT Y) const; // Returns a vertex in world space.

	FTerrainPatch GetPatch(INT X,INT Y) const;
	FVector GetCollisionVertex(const FTerrainPatch& Patch,UINT PatchX,UINT PatchY,UINT SubX,UINT SubY) const;

	const BYTE Alpha(INT AlphaMapIndex,INT X,INT Y) const;	// If AlphaMapIndex == INDEX_NONE, returns 0.
	BYTE& Alpha(INT& AlphaMapIndex,INT X,INT Y);			// If AlphaMapIndex == INDEX_NONE, creates a new alphamap and places the index in AlphaMapIndex.

	FLOAT GetCachedDisplacement(INT X,INT Y,INT SubX,INT SubY) const;

	// GetCachedMaterial - Returns a cached terrain material containing a given set of weighted materials.

	FTerrainMaterialResource* GetCachedMaterial(const FTerrainMaterialMask& Mask);

	// GetCachedMaterialInstance

	FMaterialInstance* GetCachedMaterialInstance();
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=SpriteComponent0
		Sprite=Texture2D'EngineResources.S_Terrain'
		HiddenGame=True
	End Object
	Components(0)=SpriteComponent0
	DrawScale3D=(X=512.0,Y=512.0,Z=512.0)
	bCollideActors=True
	bBlockActors=True
	bWorldGeometry=True
	bStatic=True
	bNoDelete=True
	bHidden=False
	MaxTesselationLevel=4
	TesselationDistanceScale=1.0
	StaticLightingResolution=4
}