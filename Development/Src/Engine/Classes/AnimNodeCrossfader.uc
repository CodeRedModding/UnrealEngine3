/** 
 * AnimNodeCrossfader
 * This single node allows to crossfade between 2 animations through script control.
 * A typical usage scenario would be to blend between 2 player idle animations.
 * This blend requires 2 AnimNodeSequence as childs, you cannot connect 2 blends nor any other node types.
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class AnimNodeCrossfader extends AnimNodeBlend
	native(Anim)
	hidecategories(Object);

// @todo
// - check for SetAnim in case it fails
// - make sure defaults are valid
// - verify both childs are AnimNodeSequences.

cpptext
{
	// UAnimNode interface
	virtual	void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );
	UAnimNodeSequence *GetActiveChild();
}

//
// Internal (C++) variables
//

/** true if not blending out of the current one shot anim. Anim will just freeze at last frame */
var const	bool				bDontBlendOutOneShot;

/** Blend Out time for current One Shot anim */
var const	float				PendingBlendOutTimeOneShot;

//
// Exposed (script modifable) parameters
//

/** default animation sequence played upon startup */
var()	name	DefaultAnimSeqName;

/**
 * Play a One Shot animation.
 *
 * @param	AnimSeqName		Name of animation sequence to play
 * @param	BlendInTime		time to blend from current animation to this (new) one.
 * @param	BlendOutTime	time to blend from this animation (before it finishes playing) back to the previous one.
 * @param	bDontBlendOut	if true, animation will freeze at last frame, and not blend back to the old one.
 * @param	Rate			Playing rate of animation.
 */
native final function PlayOneShotAnim
(
				name	AnimSeqName,
	optional	float	BlendInTime, 
	optional	float	BlendOutTime,
	optional	bool	bDontBlendOut,
	optional	float	Rate
);

/**
 * Blend to a looping animation.
 *
 * @param	AnimSeqName	Name of animation sequence to play.
 * @param	BlendInTime		time to blend from current animation to this (new) one.
 * @param	Rate			Playing rate of animation.
 */
native final function BlendToLoopingAnim
(
				name	AnimSeqName,
	optional	float	BlendInTime,
	optional	float	Rate
);

/**
 * Get Animation sequence currently playing 
 *
 * @return	animation sequence currently being played.
 */
native final function Name				GetActiveAnimSeq();

/**
 * Get active AnimNodeSequence child. To access animation properties and control functions.
 *
 * @return	AnimNodeSequence currently playing.
 */
native final function AnimNodeSequence	GetActiveChild();

defaultproperties
{
	Begin Object Class=AnimNodeSequence Name=anAnimSeq1
		bLooping=true
		bPlaying=true
	End object

	Begin Object Class=AnimNodeSequence Name=anAnimSeq2
	End object

	Children(0)=(Anim=anAnimSeq1)
	Children(1)=(Anim=anAnimSeq2)
}