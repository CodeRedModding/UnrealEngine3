/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Allows a chain of bones to 'trail' behind its head.
 */
class SkelControlTrail extends SkelControlBase
	native(Anim);

cpptext
{
	// USkelControlBase interface
	virtual void TickSkelControl(FLOAT DeltaSeconds, USkeletalMeshComponent* SkelComp);
	virtual void GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices);
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);	
}
 
/** Number of bones above the active one in the hierarchy to modify. */
var(Trail)		int		ChainLength;

/** Axis of the bones to point along trail. */
var(Trail)		EAxis	ChainBoneAxis;

/** Invert the direction specified in ChainBoneAxis. */
var(Trail)		bool	bInvertChainBoneAxis;

/** Limit the amount that a bone can stretch from its ref-pose length. */
var(Trail)		bool	bLimitStretch;

/** How quickly we 'relax' the bones to their animated positions. */
var(Trail)		float	TrailRelaxation;

/** If bLimitStretch is true, this indicates how long a bone can stretch beyond its length in the ref-pose. */
var(Trail)		float	StretchLimit;

/** 'Fake' velocity applied to bones. */
var(Trail)		vector	FakeVelocity;

/** Whether 'fake' velocity should be applied in actor or world space. */
var(Trail)		bool	bActorSpaceFakeVel;

/** Internal use - we need the timestep to do the relaxation in CalculateNewBoneTransforms. */
var				float	ThisTimstep;

/** Did we have a non-zero ControlStrength last frame. */
var				bool			bHadValidStrength;

/** Component-space locations of the bones from last frame. Each frame these are moved towards their 'animated' locations. */
var				transient array<vector>	TrailBoneLocations;

/** LocalToWorld used last frame, used for building transform between frames. */
var				transient matrix		OldLocalToWorld;

defaultproperties
{
	ChainLength=2
	ChainBoneAxis=AXIS_X
	TrailRelaxation=10.0
}
