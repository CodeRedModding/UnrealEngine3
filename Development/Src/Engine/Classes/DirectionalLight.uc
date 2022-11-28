class DirectionalLight extends Light
	placeable;

defaultproperties
{
	Begin Object Class=DirectionalLightComponent Name=DirectionalLightComponent0
	End Object
	Components.Add(DirectionalLightComponent0)
	LightComponent=DirectionalLightComponent0

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=150,G=200,B=255)
	End Object
	Components.Add(ArrowComponent0)

	Rotation=(Pitch=-16384,Yaw=0,Roll=0)
	DesiredRotation=(Pitch=-16384,Yaw=0,Roll=0)
}