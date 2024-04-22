//=============================================================================
// VolumePathNode
// Useful for flying or swimming
// Defines "reachable" area by growing collision cylinder from initial
// radius/height specified by LD, until an obstruction is reached.
// VolumePathNodes can reach any NavigationPath within their volume, as
// well as other VolumePathNodes with overlapping cylinders.
// NavigationPoints directly below the volumepathnode cylinder will also
// be tested for connectivity during path building.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class VolumePathNode extends PathNode
	native;

/** when path building, the cylinder starts at this size and does traces/point checks to refine
 * to a size that isn't embedded in world geometry
 * can be modified by LDs to adjust building behavior
 */
var() float StartingRadius, StartingHeight;

cpptext
{
	virtual UBOOL ReachedBy(APawn* P, const FVector& TestPosition, const FVector& Dest);
	virtual UBOOL ShouldBeBased();
	virtual void InitForPathFinding();
#if WITH_EDITOR
	virtual void addReachSpecs(AScout *Scout, UBOOL bOnlyChanged);
	virtual void ReviewPath(APawn* Scout);
	virtual UBOOL CanPrunePath(INT index);
#endif
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.VolumePath'
	End Object

	bNoAutoConnect=true
	DrawScale=+1.0
	bFlyingPreferred=true
	bVehicleDestination=true
	bNotBased=true
	bBuildLongPaths=false

	StartingRadius=2000.0
	StartingHeight=2000.0
}
