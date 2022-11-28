//=============================================================================
// Concrete Point light class.
//=============================================================================
class PointLight extends Light
	placeable;

defaultproperties
{
	Begin Object Class=PointLightComponent Name=PointLightComponent0
	End Object
	Components.Add(PointLightComponent0)
	LightComponent=PointLightComponent0
}