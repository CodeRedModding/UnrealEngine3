class StaticMeshComponent extends MeshComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var() const StaticMesh			StaticMesh;
var native const pointer		MeshObject;
var() Color						WireframeColor;

var native const array<int>		StaticLights;
var native const array<int>		StaticLightMaps;
var const array<LightComponent>	IgnoreLights;

defaultproperties
{
	CollideActors=True
	BlockActors=True
	BlockZeroExtent=True
	BlockNonZeroExtent=True
	BlockRigidBody=True
	WireframeColor=(R=0,G=255,B=255,A=255)
}