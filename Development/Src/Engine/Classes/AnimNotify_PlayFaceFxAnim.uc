
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * Play a facial animation on a character.
 */

class AnimNotify_PlayFaceFXAnim extends AnimNotify_Scripted;

/** Reference to FaceFX AnimSet package the animation is in */
var()	FaceFXAnimSet	FaceFXAnimSetRef;  // @todo:  we need to none this out so there are no hard references to FaceFX AnimSets
var()	String			GroupName;
var()	String			AnimName;

/** The sound cue to play for the this animation**/
var() SoundCue SoundCueToPlay;

/** If a FaceFX animation is already playing, then override if TRUE, skip is FALSE */
var() bool		bOverridePlayingAnim;
/** Chance to play. 0 - 1.f */
var() float		PlayFrequency;

event Notify(Actor Owner, AnimNodeSequence AnimSeqInstigator)
{

	// If this animation doesn't play all the time
	// See if we should play or ignore it!
	if( PlayFrequency < 1.f )
	{
		if( FRand() > PlayFrequency )
		{
			return;
		}
	}
	else if( PlayFrequency > 1.f )
	{
		`log("Play FaceFX animation from notify" @ AnimSeqInstigator.AnimSeqName @ "for" @ Owner @ "GroupName:" @ GroupName @ "AnimName:" @ AnimName);
		`log(" PlayFrequency > 1.0 is useless. Chance to play valid range is from 0.0 to 1.0.");
	}

	if( Owner != None )
	{
		//`log(Self @ "Play FaceFX animation from notify" @ AnimSeqInstigator.AnimSeqName @ "for" @ Owner @ "GroupName:" @ GroupName @ "AnimName:" @ AnimName @ "(bOverridePlayingAnim:"$ bOverridePlayingAnim @ "IsActorPlayingFaceFXAnim:" $ Owner.IsActorPlayingFaceFXAnim() $")");

		// If actor can (is able to) play
		if (Owner.CanActorPlayFaceFXAnim())
		{
			// If a Face FX animation is already playing, should we override it?
			if( bOverridePlayingAnim || !Owner.IsActorPlayingFaceFXAnim())
			{
				if (Owner.PlayActorFaceFXAnim(FaceFXAnimSetRef, GroupName, AnimName, SoundCueToPlay) == FALSE)
				{
					`log(AnimSeqInstigator.AnimSeq.Outer @ "(" @ AnimSeqInstigator.AnimSeqName @ ")" @ " - PlayFaceFXAnim notifier failed. Verify if this notifier is valid.",,'DevFaceFX');
				}
			}
		}
	}
}

defaultproperties
{
	bOverridePlayingAnim=TRUE
	PlayFrequency=1.f
}
