/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CylinderComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	editinlinenew;

var() const export float	CollisionHeight;
var() const export float	CollisionRadius;

/** Color used to draw the cylinder. */
var() const	color			CylinderColor;

/**	Whether to draw the red bounding box for this cylinder. */
var		const bool			bDrawBoundingBox;

/** If TRUE, this cylinder will always draw when SHOW_Collision is on, even if CollideActors is FALSE. */
var		const bool			bDrawNonColliding;

/** If TRUE, this cylinder will always draw when the actor is selected. */
var		const bool			bAlwaysRenderIfSelected;

native final function SetCylinderSize(float NewRadius, float NewHeight);

// The rotation part of the local-to-world transformation has no effect on the cylinder; it is always
// assumed to be aligned with the z-axis. The translation part is however taken into consideration.

defaultproperties
{
	HiddenGame=TRUE
	BlockZeroExtent=true
	BlockNonZeroExtent=true
	CollisionRadius=+00022.000000
	CollisionHeight=+00022.000000
	bAcceptsLights=false
	bCastDynamicShadow=false
	CylinderColor=(R=223,G=149,B=157,A=255)
	bDrawBoundingBox=TRUE
	bAlwaysRenderIfSelected=false
	bAcceptsDynamicDecals=FALSE
}
