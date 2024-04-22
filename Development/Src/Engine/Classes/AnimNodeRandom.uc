
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNodeRandom extends AnimNodeBlendList
	native(Anim)
	hidecategories(Object);

cpptext
{
	virtual void TickAnim(FLOAT DeltaSeconds);
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	/** A child has been added, update RandomInfo accordingly */
	virtual void OnAddChild(INT ChildNum);
	/** A child has been removed, update RandomInfo accordingly */
	virtual void OnRemoveChild(INT ChildNum);

	/** Notification to this blend that a child UAnimNodeSequence has reached the end and stopped playing. Not called if child has bLooping set to true or if user calls StopAnim. */
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child, FLOAT PlayedTime, FLOAT ExcessTime);

	/** Notification when node becomes relevant. */
	virtual void OnBecomeRelevant();

	INT		PickNextAnimIndex();
	void	PlayPendingAnimation(FLOAT BlendTime=0.f, FLOAT StartTime=0.f);
}

struct native RandomAnimInfo
{
	/** Chance this child will be selected */
	var() float		Chance;
	/** Minimum number of loops to play this animation */
	var() Byte		LoopCountMin;
	/** Maximum number of loops to play this animation */
	var() Byte		LoopCountMax;
	/** Blend in time for this child */
	var() float		BlendInTime;
	/** Animation Play Rate Scale */
	var() Vector2D	PlayRateRange;
	/** If it's a still frame, don't play animation. Just randomly pick one, and stick to it until we lose focus */
	var() bool		bStillFrame;

	/** Number of loops left to play for this round */
	var transient byte	LoopCount;
	/** Keep track of last position */
	var transient float LastPosition;

	structdefaultproperties
	{
		Chance=1.f
		LoopCountMin=0
		LoopCountMax=0
		BlendInTime=0.25f
		PlayRateRange=(X=1.f,Y=1.f)
	}
};

var() editfixedsize editinline Array<RandomAnimInfo> RandomInfo;

/** Pointer to AnimNodeSequence currently playing random animation. */
var transient	AnimNodeSequence	PlayingSeqNode;
var transient	INT					PendingChildIndex;
var transient   bool                bPickedPendingChildIndex;

defaultproperties
{
	ActiveChildIndex=-1
	PendingChildIndex=-1

	CategoryDesc = "Random"
}
