
/**
 *	Simple controller that replaces or adds to the translation/rotation of a single bone.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SkelControlSingleBone extends SkelControlBase
	native(Anim);

cpptext
{
	// USkelControlBase interface
	virtual void GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices);
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);

	virtual INT GetWidgetCount();
	virtual FBoneAtom GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex);
	virtual void HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec);
}

/** Whether to modify the translation of this bone. */
var(Adjustments)	bool				bApplyTranslation;
/** Whether to modify the translation of this bone. */
var(Adjustments)	bool				bApplyRotation;

/** If false, replaces rotation with BoneRotation. If true, adds to existing rotation. */
var(Translation)	bool				bAddTranslation;
/** New translation of bone to apply. */
var(Translation)	vector				BoneTranslation;
/** Reference frame to apply BoneTranslation in. */
var(Translation)	EBoneControlSpace	BoneTranslationSpace;
/** Name of bone used if BoneTranslationSpace is BCS_OtherBoneSpace. */
var(Translation)	name				TranslationSpaceBoneName;

/** If false, replaces rotation with BoneRotation. If true, adds to existing rotation. */
var(Rotation)		bool				bAddRotation;
/** If bAddRotation and BCS_WorldSpace, then remove mesh relative rotation */
var(Rotation)		bool				bRemoveMeshRotation;
/** New rotation of bone to apply. */
var(Rotation)		rotator				BoneRotation;
/** Reference frame to apply BoneRotation in. */
var(Rotation)		EBoneControlSpace	BoneRotationSpace;
/** Name of bone used if BoneRotationSpace is BCS_OtherBoneSpace. */
var(Rotation)		name				RotationSpaceBoneName;

defaultproperties
{
	CategoryDesc = "Single Bone"
}
