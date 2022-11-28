//=============================================================================
// ClipMarker.
//
// These are markers for the brush clip mode.  You place 2 or 3 of these in
// the level and that defines your clipping plane.
//
// These should NOT be manually added to the level.  The editor adds and
// deletes them on it's own.
//
//=============================================================================
class ClipMarker extends Keypoint
	placeable
	native;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_ClipMarker'
	End Object
	
	bEdShouldSnap=true
	bStatic=true
}
