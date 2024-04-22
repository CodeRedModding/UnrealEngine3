/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawQuadComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/**
 *	Utility component for drawing a textured quad face. 
 *  Origin is at the component location, frustum points down position X axis.
 */

/** Texture source to draw on quad face */
var() Texture		Texture;
/** Width of quad face */
var() float			Width;
/** Height of quad face */
var() float			Height;

defaultproperties
{
	Width=100
	Height=100

	HiddenGame=true
	CollideActors=false
}
