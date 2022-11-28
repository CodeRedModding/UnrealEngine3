//=============================================================================
// Ambient sound, sits there and emits its sound.
//=============================================================================
class AmbientSound extends Keypoint;

var() editconst const AudioComponent AudioComponent;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Ambient'
	End Object

	Begin Object Class=AudioComponent Name=AudioComponent0
		bAutoPlay=true
	End Object
	AudioComponent=AudioComponent0
	Components.Add(AudioComponent0)

	RemoteRole=ROLE_None
}
