/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class DrawCylinderComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var()	color			CylinderColor;
var()	material		CylinderMaterial;
var()	float			CylinderRadius;
var()	float			CylinderTopRadius;
var()	float			CylinderHeight;
var()	float			CylinderHeightOffset;
var()	int				CylinderSides;
var()	bool			bDrawWireCylinder;
var()	bool			bDrawLitCylinder;
var()	bool			bDrawOnlyIfSelected;

defaultproperties
{
	CylinderColor=(R=255,G=0,B=0,A=255)
	CylinderRadius=100.0
	CylinderTopRadius=100.0
	CylinderHeight=100.0
	CylinderHeightOffset=0.0
	CylinderSides=16
	bDrawWireCylinder=true

	HiddenGame=True
}
