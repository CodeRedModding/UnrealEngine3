class Shape extends Brush
	native
	placeable;

/** 
 * Shape
 *
 * Created by:	Warren Marshall
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * Represents a 2 dimensional polygon that can be extruded/revolved
 * into a valid 3 dimensional builder brush.
 */
 
cpptext
{
	virtual UBOOL IsAShape() {return true;}
	virtual void Spawned();
	virtual FColor GetWireColor();
}

defaultproperties
{
}
