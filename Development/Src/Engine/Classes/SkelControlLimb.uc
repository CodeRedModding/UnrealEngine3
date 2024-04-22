class SkelControlLimb extends SkelControlBase
	native(Anim);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	Controller Specifically designed for 2-bone limbs with an elbow-like joint in the middle.
 *	Should be set up to act on the bone at the end of the limb - ie the hand in the case of an arm.
 */

cpptext
{
	// USkelControlBase interface
	virtual void GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices);
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);

	virtual INT GetWidgetCount();
	virtual FBoneAtom GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex);
	virtual void HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec);
	virtual void DrawSkelControl3D(const FSceneView* View, FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, INT BoneIndex);
}

/** Where you want the controlled bone to be. Will be placed as close as possible within the constraints of the limb. */
var(Effector)	vector					EffectorLocation;

/** Reference frame that the DesiredLocation is defined in. */
var(Effector)	EBoneControlSpace		EffectorLocationSpace;

/** Name of bone used if DesiredLocationSpace is BCS_OtherBoneSpace or BCS_BaseMeshSpace. */
var(Effector)	name					EffectorSpaceBoneName;

/** Point in space where the joint should move towards as it bends. */
var(Joint)		vector					JointTargetLocation;

/** Reference frame in which JointTargetLocation is defined. */
var(Joint)		EBoneControlSpace		JointTargetLocationSpace;
/** Name of bone used if JointTargetLocationSpace is BCS_OtherBoneSpace. */
var(Joint)		name					JointTargetSpaceBoneName;
/** Apply an Offset to the Joint (Elbow) bone. This will create stretching! */
var(Joint)      Vector                  JointOffset;
/** Space this offset is in */
var(Joint)      EBoneControlSpace       JointOffsetSpace;
/** Optional bone name to be used by JoinOffsetSpace */
var(Joint)      name                    JointOffsetBoneName;

/** Axis of graphical bone to align along the length of the bone. */
var(Limb)		EAxis					BoneAxis;

/** Axis of graphical bone to align along the hinge axis of the joint. */
var(Limb)		EAxis					JointAxis;

/** If we want to invert BoneAxis when constructing the transform for the bones. */
var(Limb)		bool					bInvertBoneAxis;

/** If we want to invert JointAxis when constructing the transform for the bones. */
var(Limb)		bool					bInvertJointAxis;
/** Experiment to Rotate Joint Bone, rather than creating a new Matrix for it. */
var(Limb)       bool                    bRotateJoint;

/**
 *	If true, modify the relative rotation between the end 'effector' bone and its parent bone. If false,
 *	the rotation of the end bone will not be modified by this controller.
 */
var(Limb)		bool					bMaintainEffectorRelRot;

/** If true, rotation of effector bone is copied from the bone specified by EffectorSpaceBoneName. */
var(Limb)		bool					bTakeRotationFromEffectorSpace;

/** Is Limb allowed to stretch to reach its target? */
var()	bool		bAllowStretching;
/**
 * Stretching limits.
 * X represents the relative length of the limb at which it will start stretching.
 * Y represents the maximum stretch factor allowed.
 * For example (X=0.5,Y=2.f) will mean that the limb is allowed to stretch up to twice its length.
 * And it will start stretching when shoulder to effector distance is half of the full arm length.
 */
var()	Vector2D	StretchLimits;
/** Name of Roll Bone to stretch as well. */
var()	Name		StretchRollBoneName;

defaultproperties
{
	EffectorLocationSpace=BCS_WorldSpace
	JointTargetLocationSpace=BCS_WorldSpace

	BoneAxis=AXIS_X
	JointAxis=AXIS_Y

	StretchLimits=(X=0.71f,Y=1.2f)

	CategoryDesc = "Limb"

	bIgnoreWhenNotRendered=TRUE // IK Controllers can typically be skipped when mesh is not rendered.
}
