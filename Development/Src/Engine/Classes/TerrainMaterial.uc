class TerrainMaterial extends Object
	native(Terrain)
	hidecategories(Object);

var matrix					LocalToMapping;

enum ETerrainMappingType
{
	TMT_Auto,
	TMT_XY,
	TMT_XZ,
	TMT_YZ
};

var(Material) ETerrainMappingType	MappingType;
var(Material) float					MappingScale;
var(Material) float					MappingRotation;
var(Material) float					MappingPanU,
									MappingPanV;

var(Material) MaterialInstance		Material;

var(Displacement) Texture2D			DisplacementMap;
var(Displacement) float				DisplacementScale;

struct native TerrainFoliageMesh
{
	var() StaticMesh		StaticMesh;
	var() MaterialInstance	Material;
	var() int				Density;
	var() float				MaxDrawRadius,
							MinTransitionRadius,
							MinScale,
							MaxScale;
	var() int				Seed;
	var() float				SwayScale;
	var() bool				StaticLighting;

	structdefaults
	{
		MaxDrawRadius=1024.0
		MinScale=1.0
		MaxScale=1.0
		SwayScale=1.0
		StaticLighting=True
	}
};

var(Foliage) array<TerrainFoliageMesh>	FoliageMeshes;

cpptext
{
	// UpdateMappingTransform

	void UpdateMappingTransform();

	// Displacement sampler.

	FLOAT GetDisplacement(FLOAT U,FLOAT V) const;

	// UObject interface.

	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void DrawThumbnail(EThumbnailPrimType PrimType,INT X,INT Y,struct FChildViewport* Viewport,struct FRenderInterface* RI,FLOAT Zoom,UBOOL ShowBackground,FLOAT ZoomPct,INT InFixedSz);
	virtual FThumbnailDesc GetThumbnailDesc(FRenderInterface* RI, FLOAT Zoom, INT InFixedSz);
	virtual INT GetThumbnailLabels(TArray<FString>* Labels);
}

defaultproperties
{
	MappingScale=4.0
	DisplacementScale=0.25
}

