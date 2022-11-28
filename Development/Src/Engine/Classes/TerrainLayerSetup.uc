class TerrainLayerSetup extends Object
	native(Terrain)
	hidecategories(Object)
	collapsecategories;

struct FilterLimit
{
	var() bool	Enabled;
	var() float	Base,
				NoiseScale,
				NoiseAmount;
};

struct TerrainFilteredMaterial
{
	var() bool			UseNoise;
	var() float			NoiseScale,
						NoisePercent;

	var() FilterLimit	MinHeight;
	var() FilterLimit	MaxHeight;

	var() FilterLimit	MinSlope;
	var() FilterLimit	MaxSlope;

	var() float				Alpha;
	var() TerrainMaterial	Material;

	structdefaults
	{
		Alpha=1.0
	}
};

var() array<TerrainFilteredMaterial>	Materials;

cpptext
{
	// UObject interface.

	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void DrawThumbnail(EThumbnailPrimType PrimType,INT X,INT Y,struct FChildViewport* Viewport,struct FRenderInterface* RI,FLOAT Zoom,UBOOL ShowBackground,FLOAT ZoomPct,INT InFixedSz);
	virtual FThumbnailDesc GetThumbnailDesc(FRenderInterface* RI, FLOAT Zoom, INT InFixedSz);
	virtual INT GetThumbnailLabels(TArray<FString>* Labels);
}
