class AnimNodeBlendPerBone extends AnimNodeBlend
	native(Anim)
	hidecategories(Object);

/** Weight scaling for each bone of the skeleton. Must be same size as RefSkeleton of SkeletalMesh. If all 0.0, no animation can ever be drawn from Child2. */
var		array<float>	Child2PerBoneWeight;

/** Used in InitAnim, so you can set up partial blending in the defaultproperties. See SetChild2StartBone. */
var		name		InitChild2StartBone;

/** Used in InitAnim, so you can set up partial blending in the defaultproperties.  See SetChild2StartBone. */
var		float		InitPerBoneIncrease;

cpptext
{
	// AnimNode interface
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	virtual void GetBoneAtoms( TArray<FBoneAtom>& Atoms);

	// AnimNodeBlendPerBone interface

	/** Utility for creating the Child2PerBoneWeight array. Starting from the named bone, walk down the heirarchy increasing the weight by PerBoneIncrease each step. */
	virtual void SetChild2StartBone( FName StartBoneName, FLOAT PerBoneIncrease=1.f );
}

native final function SetChild2StartBone( name StartBoneName, optional float PerBoneIncrease );

defaultproperties
{
	InitPerBoneIncrease=1.0
}