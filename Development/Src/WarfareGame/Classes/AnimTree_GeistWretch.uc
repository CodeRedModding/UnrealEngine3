class AnimTree_GeistWretch extends AnimTree;

/**
 * Geist Wretch Animation Tree
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

defaultproperties
{
			Begin Object Class=AnimNodeSequence Name=aWalkFwd
				AnimSeqName=WalkForward01
				bLooping=true
				bPlaying=true
			End Object
		
			Begin Object Class=AnimNodeSequence Name=aWalkBwd
				AnimSeqName=WalkBackward01
				bLooping=true
				bPlaying=true
			End Object
		
			Begin Object Class=AnimNodeSequence Name=aWalkLft
				AnimSeqName=WalkLeft01
				bLooping=true
				bPlaying=true
			End Object
		
			Begin Object Class=AnimNodeSequence Name=aWalkRt
				AnimSeqName=WalkRight01
				bLooping=true
				bPlaying=true
			End Object
		
			Begin Object Class=AnimNodeBlendDirectional Name=aWalkDir
				Children(0)=(Anim=aWalkFwd)
				Children(1)=(Anim=aWalkBwd)
				Children(2)=(Anim=aWalkLft)
				Children(3)=(Anim=aWalkRt)
			End Object
		
			Begin Object Class=AnimNodeSequence Name=aWalkIdle
				AnimSeqName=IdleReady_03
				bLooping=true
				bPlaying=true
			End Object
		// Walking directions ^^^^^^^^^^^^^^^^^

		Begin Object Class=AnimNodeBlendBySpeed Name=aWalkMove
			Children(0)=(Anim=aWalkIdle)
			Children(1)=(Anim=aWalkDir)
			BlendUpTime=0.15
			BlendDownTime=0.25
			BlendDownPerc=0.1
			Constraints=(0,180)
		End Object
		// Walking ^^^^^^^^^^^^^^^
		
		Begin Object Class=AnimNodeSequence Name=anFire
			AnimSeqName=JumpForward_01
			bLooping=true
			bPlaying=true
		End object
		// Attack ^^^^^^^^^^^^^^^

	Begin Object Class=WarAnim_CoverFireBlendNode Name=aBaseBlend
		Children(0)=(Anim=aWalkMove)
		Children(1)=(Anim=anFire)
		InitChild2StartBone=Spine
		InitPerBoneIncrease=0.2
		Child2Weight=(Weight=0.0)
	End Object

	// Top of the node
	Children(0)=(Anim=aBaseBlend)
	// End Root
	//============================================================
}