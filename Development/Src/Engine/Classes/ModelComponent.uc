class ModelComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	editinlinenew;

var const Level				Level;
var const int				ZoneIndex;
var native const pointer	Model;
var native const pointer	MeshObject;
var native const array<int>	StaticLights;
var const array<LightComponent>	IgnoreLights;

defaultproperties
{
	CastShadow=True
}