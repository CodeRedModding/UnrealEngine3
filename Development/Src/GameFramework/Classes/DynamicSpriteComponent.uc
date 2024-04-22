/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DynamicSpriteComponent extends SpriteComponent
	DontCollapseCategories
	native;

/** Animated Scale. Relative to DrawScale. */
var() InterpCurveFloat AnimatedScale;

/** Animated color + alpha */
var() InterpCurveLinearColor AnimatedColor;

/** Animated 2D position (screen space). Relative to StartPosition. */
var() InterpCurveVector2D AnimatedPosition;

/** 3D world space offset from Location */
var() Vector LocationOffset;

/** How many times to loop (-1 = infinite) */
var() int LoopCount;


cpptext
{
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
}


defaultproperties
{
	LoopCount=-1
}