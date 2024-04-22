/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawFrustumComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/**
 *	Utility component for drawing a view frustum. Origin is at the component location, frustum points down position X axis.
 */

/** Color to draw the wireframe frustum. */
var()	color			FrustumColor;

/** Angle of longest dimension of view shape. 
  * If the angle is 0 then an orthographic projection is used */
var()	float			FrustumAngle;

/** Ratio of horizontal size over vertical size. */
var()	float			FrustumAspectRatio;

/** Distance from origin to start drawing the frustum. */
var()	float			FrustumStartDist;

/** Distance from origin to stop drawing the frustum. */
var()	float			FrustumEndDist;

/** optional texture to show on the near plane */
var()	Texture			Texture;

defaultproperties
{
	FrustumColor=(R=255,G=0,B=255,A=255)
	FrustumAngle=90.0
	FrustumAspectRatio=AspectRatio4x3
	FrustumStartDist=100
	FrustumEndDist=1000

	HiddenGame=true
	CollideActors=false
}
