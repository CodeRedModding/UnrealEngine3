//=============================================================================
// PathNode.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class PathNode extends NavigationPoint
	placeable
	native;

cpptext
{
#if WITH_EDITOR
	virtual INT AddMyMarker(AActor *S);
#endif
}

simulated event string GetDebugAbbrev()
{
	return "PN";
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.S_Pickup'
	End Object
}
