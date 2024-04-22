
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNodeAdditiveBlending extends AnimNodeBlend
	native(Anim);

/**
 * if TRUE, pass through (skip additive animation blending) when mesh is not rendered
 */
var(Performance) bool	bPassThroughWhenNotRendered;

cpptext
{
	virtual void InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent);
	virtual	void TickAnim(FLOAT DeltaSeconds);
	void GetChildAtoms(INT ChildIndex, FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);
	virtual void GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);
}

/**
 * Overridden so we can keep child zero weight at 1.
 */
native function SetBlendTarget( float BlendTarget, float BlendTime );

defaultproperties
{
	bPassThroughWhenNotRendered=TRUE
	bFixNumChildren=TRUE
	Children(0)=(Name="Base Anim Input",Weight=1.f)
	Children(1)=(Name="Additive Anim Input",Weight=1.f)
	Child2Weight=1.f
	Child2WeightTarget=1.f

	CategoryDesc = "Additive"
}
