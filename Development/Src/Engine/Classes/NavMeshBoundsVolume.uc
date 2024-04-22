/**
 * this volume only blocks the path builder - it has no gameplay collision
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class NavMeshBoundsVolume extends Volume
	placeable;


defaultproperties
{
	BrushColor=(R=74,G=74,B=74,A=255)
	bColored=TRUE
	bCollideActors=false
	bStatic=true
	bNoDelete=true
}
