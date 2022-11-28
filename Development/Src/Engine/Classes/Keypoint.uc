//=============================================================================
// Keypoint, the base class of invisible actors which mark things.
//=============================================================================
class Keypoint extends Actor
	abstract
	placeable
	native;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Keypoint'
	End Object

	bStatic=true
	bHidden=true
}
