//=============================================================================
// Concrete spot light class.
//=============================================================================
class SpotLight extends Light
	placeable;

defaultproperties
{
	Begin Object Class=SpotLightComponent Name=SpotLightComponent0
	End Object
	Components.Add(SpotLightComponent0)
	LightComponent=SpotLightComponent0

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=150,G=200,B=255)
	End Object
	Components.Add(ArrowComponent0)

	Rotation=(Pitch=-16384,Yaw=0,Roll=0)
	DesiredRotation=(Pitch=-16384,Yaw=0,Roll=0)
}