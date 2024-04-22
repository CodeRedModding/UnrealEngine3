/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Options for the user to control how static meshes are dropped into the editor.
 */
class StaticMeshMode_Options
	extends Object
	hidecategories(Object)
	native;

/** Settings that you want to apply to all static meshes that get placed. */
var(StaticMeshSettings)	ECollisionType		CollisionType;

/** Pre-rotation values to correct meshes with weird orientations. */
var()	rotator		PreRotation;

/** Range for tweaks to rotation. */
var()	rotator		RotationMin;
var()	rotator		RotationMax;

/** Range for tweaks to DrawScale3D. */
var()	vector		Scale3DMin;
var()	vector		Scale3DMax;

/** Range for tweaks to DrawScale. */
var()	float		ScaleMin;
var()	float		ScaleMax;

defaultproperties
{
	CollisionType=COLLIDE_BlockWeapons
	Scale3DMin=(X=1.0,Y=1.0,Z=1.0)
	Scale3DMax=(X=1.0,Y=1.0,Z=1.0)
	ScaleMin=1.0
	ScaleMax=1.0
}
