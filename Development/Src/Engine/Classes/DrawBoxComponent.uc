/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawBoxComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var()	color			BoxColor;
var()	material		BoxMaterial;
var()	vector			BoxExtent;
var()	bool			bDrawWireBox;
var()	bool			bDrawLitBox;
var()	bool			bDrawOnlyIfSelected;

defaultproperties
{
	BoxColor=(R=255,G=0,B=0,A=255)
	BoxExtent=(X=200.0, Y=200.0, Z=200.0)
	bDrawWireBox=true

	HiddenGame=True
}
