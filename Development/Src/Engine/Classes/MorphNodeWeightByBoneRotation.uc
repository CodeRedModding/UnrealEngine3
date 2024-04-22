
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class MorphNodeWeightByBoneRotation extends MorphNodeWeightBase
	dependson(MaterialInstanceConstant,MorphNodeWeightByBoneAngle)
	native(Anim);

/**
 * This node maps the rotation of a bone (compared to the ref skeleton)
 * to a weight scaling morph targets using used defined values.
 */

cpptext
{
	virtual void GetActiveMorphs(TArray<FActiveMorph>& OutMorphs);
	/** Draw on 3d viewport canvas when node is selected */
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas, const FSceneView* View);
	virtual void Render(const FSceneView* View, FPrimitiveDrawInterface* PDI);
	void DrawDebugCoordSystem(FPrimitiveDrawInterface* PDI, FColor Color, const FBoneAtom& LocalBoneAtom, const FBoneAtom& ParentBoneTransform);
	FQuat GetAlignedQuat(INT BoneIndex);
}

// Internal variables
var	const transient float	Angle;
var	const transient float	NodeWeight;

/** Bone Name */
var()	Name	BoneName;
/** Bone Axis to use X, Y or Z */
var()	EAxis	BoneAxis;
/** Should the bone axis be inverted? */
var()	bool	bInvertBoneAxis;

/** Array of points translating angles into morph weights */
var()	Array<BoneAngleMorph> WeightArray;

// Material Parameter control
var(Material)				bool						bControlMaterialParameter;
var(Material)				INT							MaterialSlotId;
var(Material)				Name						ScalarParameterName;
var				transient	MaterialInstanceConstant	MaterialInstanceConstant;

defaultproperties
{
	NodeConns(0)=(ConnName=In)

	BoneAxis=AXIS_Y

	WeightArray(0)=(Angle=0.f,TargetWeight=0.f)
	WeightArray(1)=(Angle=90.f,TargetWeight=1.f)
}