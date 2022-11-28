class TerrainComponent extends PrimitiveComponent
	native(Terrain)
	noexport;

var private const native array<int>	StaticLights;
var const array<LightComponent>		IgnoreLights;

var const native int		TerrainObject;
var const int				SectionBaseX,
							SectionBaseY,
							SectionSizeX,
							SectionSizeY;

var private const native array<int>	PatchBounds;
var private const native array<int>	PatchBatches;
var private const native array<int>	BatchMaterials;
var private const native int		FullBatch;

defaultproperties
{
	CollideActors=True
	BlockActors=True
	BlockZeroExtent=True
	BlockNonZeroExtent=True
	BlockRigidBody=True
	CastShadow=True
}