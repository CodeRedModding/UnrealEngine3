/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawSphereComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var()	color			SphereColor;
var()	material		SphereMaterial;
var()	float			SphereRadius;
var()	int				SphereSides;
var()	bool			bDrawWireSphere;
var()	bool			bDrawLitSphere;
var()	bool			bDrawOnlyIfSelected;

defaultproperties
{
	SphereColor=(R=255,G=0,B=0,A=255)
	SphereRadius=100.0
	SphereSides=16
	bDrawWireSphere=true

	HiddenGame=True
}
