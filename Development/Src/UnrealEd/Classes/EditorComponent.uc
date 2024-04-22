/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class EditorComponent extends PrimitiveComponent
	native
	noexport;

/** These mirror the C++ side properties. I'm making a class here so
    ModelComponent will get the defaultprops from the PrimitiveComponent base class */

	var const bool bDrawGrid;
	var const bool bDrawPivot;
	var const bool bDrawBaseInfo;
	var const bool bDrawWorldBox;
	var const bool bDrawColoredOrigin;
	var const bool bDrawKillZ;

	var const color GridColorHi;
	var const color GridColorLo;
	var const float PerspectiveGridSize;

	var const color PivotColor;
	var const float PivotSize;

	var const color BaseBoxColor;

defaultproperties
{
	DepthPriorityGroup=SDPG_UnrealEdBackground

	bDrawGrid=true
	bDrawPivot=true
	bDrawBaseInfo=true

	GridColorHi=(R=0,G=0,B=127)
	GridColorLo=(R=0,G=0,B=63)
	PerspectiveGridSize=262143.0 //HALF_WORLD_MAX1
	bDrawWorldBox=true
	bDrawColoredOrigin=false
	bDrawKillZ=true

	PivotColor=(R=255,G=0,B=0)
	PivotSize=0.02

	BaseBoxColor=(R=0,G=255,B=0)
}
