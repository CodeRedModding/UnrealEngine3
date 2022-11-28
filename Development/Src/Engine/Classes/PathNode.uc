//=============================================================================
// PathNode.
//=============================================================================
class PathNode extends NavigationPoint
	placeable
	native;

cpptext
{
	virtual UBOOL ReviewPath(APawn* Scout);
	virtual void CheckSymmetry(ANavigationPoint* Other);
	virtual INT AddMyMarker(AActor *S);
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Pickup'
	End Object
}
