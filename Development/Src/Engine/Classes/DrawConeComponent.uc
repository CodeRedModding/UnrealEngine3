/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawConeComponent extends PrimitiveComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var()	color			ConeColor;
var()	float			ConeRadius;
var()	float			ConeAngle;
var()	int				ConeSides;

cpptext
{
	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
}

defaultproperties
{
	ConeColor=(R=150,G=200,B=255,A=255)
	ConeRadius=100.0
	ConeAngle=44.0
	ConeSides=16

	HiddenGame=True
}
