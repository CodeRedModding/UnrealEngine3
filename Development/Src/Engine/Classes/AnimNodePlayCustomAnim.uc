
/**
 * Gives code control to override an AnimTree branch, with a custom animation.
 * . Normal branch is the normal tree branch (for example Human upper body).
 * . Custom branch must be connected to an AnimNodeSequence.
 * This node can then take over the upper body to play a cutom animation given various parameters.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNodePlayCustomAnim extends AnimNodeBlend
	DependsOn(AnimNodeSequence)
	native(Anim);

cpptext
{
	virtual void TickAnim(FLOAT DeltaSeconds);

	virtual INT GetNumSliders() const { return 0; }
};

/** True, when we're playing a custom animation */
var bool	bIsPlayingCustomAnim;
/** save blend out time when playing a one shot animation. */
var float	CustomPendingBlendOutTime;


/**
 * Play a custom animation.
 * Supports many features, including blending in and out.
 *
 * @param	AnimName		Name of animation to play.
 * @param	Rate			Rate animation should be played at.
 * @param	BlendInTime		Blend duration to play anim.
 * @param	BlendOutTime	Time before animation ends (in seconds) to blend out.
 *							-1.f means no blend out.
 *							0.f = instant switch, no blend.
 *							otherwise it's starting to blend out at AnimDuration - BlendOutTime seconds.
 * @param	bLooping		Should the anim loop? (and play forever until told to stop)
 * @param	bOverride		play same animation over again only if bOverride is set to true.
 *
 * @return	PlayBack length of animation.
 */
final native function float PlayCustomAnim
(
				name	AnimName,
				float	Rate,
	optional	float	BlendInTime,
	optional	float	BlendOutTime,
	optional	bool	bLooping,
	optional	bool	bOverride
);


/**
 * Play a custom animation.
 * Auto adjusts the animation's rate to match a given duration in seconds.
 * Supports many features, including blending in and out.
 *
 * @param	AnimName		Name of animation to play.
 * @param	Duration		duration in seconds the animation should be played.
 * @param	BlendInTime		Blend duration to play anim.
 * @param	BlendOutTime	Time before animation ends (in seconds) to blend out.
 *							-1.f means no blend out.
 *							0.f = instant switch, no blend.
 *							otherwise it's starting to blend out at AnimDuration - BlendOutTime seconds.
 * @param	bLooping		Should the anim loop? (and play forever until told to stop)
 * @param	bOverride		play same animation over again only if bOverride is set to true.
 */
final native function PlayCustomAnimByDuration
(
				name	AnimName,
				float	Duration,
	optional	float	BlendInTime,
	optional	float	BlendOutTime,
	optional	bool	bLooping,
	optional	bool	bOverride
);


/**
 * Stop playing a custom animation.
 * Used for blending out of a looping custom animation.
 */
final native function StopCustomAnim(float BlendOutTime);


/**
 * Set Custom animation.
 */
final function SetCustomAnim(Name AnimName)
{
	local AnimNodeSequence SeqNode;

	SeqNode = AnimNodeSequence(Children[1].Anim);
	if( SeqNode != None )
	{
		SeqNode.SetAnim(AnimName);
	}
}


/** Set bCauseActorAnimEnd flag */
final function SetActorAnimEndNotification(bool bNewStatus)
{
	local AnimNodeSequence SeqNode;

	SeqNode = AnimNodeSequence(Children[1].Anim);
	if( SeqNode != None )
	{
		SeqNode.bCauseActorAnimEnd = bNewStatus;
	}
}


/** Returns AnimNodeSequence playing the custom animation */
final function AnimNodeSequence GetCustomAnimNodeSeq()
{
	return AnimNodeSequence(Children[1].Anim);
}


/**
 * Set custom animation root bone options.
 */
final function SetRootBoneAxisOption
(
	optional ERootBoneAxis AxisX = RBA_Default,
	optional ERootBoneAxis AxisY = RBA_Default,
	optional ERootBoneAxis AxisZ = RBA_Default
)
{
	local AnimNodeSequence AnimSeq;

	AnimSeq = GetCustomAnimNodeSeq();
	if( AnimSeq != None )
	{
		AnimSeq.SetRootBoneAxisOption(AxisX, AxisY, AxisZ);
	}
	else
	{
		`Warn(GetFuncName() @ "Custom AnimNodeSequence not found for" @ Self);
	}
}


defaultproperties
{
	NodeName="CustomAnim"
	bFixNumChildren=TRUE
	Children(0)=(Name="Normal")
	Children(1)=(Name="Custom")
}
