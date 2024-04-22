/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawCapsuleComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var()	color			CapsuleColor;
var()	material		CapsuleMaterial;
var()	float			CapsuleHeight;
var()	float			CapsuleRadius;
var()	bool			bDrawWireCapsule;
var()	bool			bDrawLitCapsule;
var()	bool			bDrawOnlyIfSelected;

defaultproperties
{
	CapsuleColor=(R=255,G=0,B=0,A=255)
	CapsuleHeight=200.0
	CapsuleRadius=200.0
	bDrawWireCapsule=true

	HiddenGame=True
}
