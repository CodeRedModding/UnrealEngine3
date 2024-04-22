
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class MorphNodeWeightByBoneAngle extends MorphNodeWeightBase
	dependson(MaterialInstanceConstant)
	native(Anim);

/**
 * This node gets the shortest angle between 2 bones (0d to 180d),
 * and translates that into a weight scaling morph targets using used defined values.
 */

cpptext
{
	virtual void GetActiveMorphs(TArray<FActiveMorph>& OutMorphs);
	/** Render on 3d viewport when node is selected. */
	virtual void Render(const FSceneView* View, FPrimitiveDrawInterface* PDI);
	/** Draw on 3d viewport canvas when node is selected */
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas, const FSceneView* View);
}
 
// Internal variables
var	const transient float	Angle;
var	const transient float	NodeWeight;

/** Base Bone Name */
var(BaseBone)	Name	BaseBoneName;
/** Axis to use on Base Bone, X, Y or Z */
var(BaseBone)	EAxis	BaseBoneAxis;
/** Should the Angle bone axis be inverted? */
var(BaseBone)	bool	bInvertBaseBoneAxis;

/** Angle bone name */
var(AngleBone)	Name	AngleBoneName;
/** Axis to use on Angle Bone, X, Y or Z */
var(AngleBone)	EAxis	AngleBoneAxis;
/** Should the Angle bone axis be inverted? */
var(AngleBone)	bool	bInvertAngleBoneAxis;

// Material Parameter control
var(Material)				bool						bControlMaterialParameter;
var(Material)				INT							MaterialSlotId;
var(Material)				Name						ScalarParameterName;
var				transient	MaterialInstanceConstant	MaterialInstanceConstant;

/** Structure defining Angle to Morph weight correspondance */
struct native BoneAngleMorph
{
	var()	FLOAT	Angle;
	var()	FLOAT	TargetWeight;

	structdefaultproperties
	{
		TargetWeight=1.f
	}
};
/** Array of points translating angles into morph weights */
var()	Array<BoneAngleMorph> WeightArray;

defaultproperties
{
	NodeConns(0)=(ConnName=In)

	BaseBoneAxis=AXIS_X
	AngleBoneAxis=AXIS_X

	WeightArray(0)=(Angle=0.f,TargetWeight=0.f)
	WeightArray(1)=(Angle=180.f,TargetWeight=1.f)
}

