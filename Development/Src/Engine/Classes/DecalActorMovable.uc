/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class DecalActorMovable extends DecalActorBase
	native(Decal)
	placeable;

defaultproperties
{
	Begin Object Name=NewDecalComponent
		bMovableDecal=TRUE
	End Object	

	bStatic=FALSE
	bNoDelete=TRUE
	bMovable=TRUE
	bHardAttach=TRUE
}
