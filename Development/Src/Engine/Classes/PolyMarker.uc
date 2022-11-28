//=============================================================================
// PolyMarker.
//
// These are markers for the polygon drawing mode.
//
// These should NOT be manually added to the level.  The editor adds and
// deletes them on it's own.
//
//=============================================================================
class PolyMarker extends Keypoint
	placeable
	native;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_PolyMarker'
	End Object

	bEdShouldSnap=true
	bStatic=true
}
