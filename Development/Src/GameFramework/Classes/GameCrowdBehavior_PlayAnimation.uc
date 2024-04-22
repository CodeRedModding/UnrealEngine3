/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdBehavior_PlayAnimation extends GameCrowdAgentBehavior
	native
	placeable
	dependsOn(GameCrowdAgent);

/** List of animations to play */
var() Array<name> AnimationList;

/** Time to blend into next animation. */
var() float BlendInTime;

/** Time to blend out of animation. */
var() float BlendOutTime;

/** Whether to use root motion. */
var() bool bUseRootMotion;

/** If true, face player before starting animation. */
var() bool bLookAtPlayer;

/** Used by kismet PlayAgentAnimation */
var Actor CustomActionTarget;

/** If true, loop the animation in the list specified by LoopIndex. */
var() bool bLooping;

/** Which animation to loop in AnimationList if bLooping == TRUE */
var() int LoopIndex;

/** How long to loop the animation if bLooping == TRUE, -1.f == infinite */
var() float LoopTime;

/** Whether should blend between animations in the list.  Set True if they don't match at start/end */
var() bool bBlendBetweenAnims;

/** Kismet AnimSequence that spawned this behavior (optional) */
var SeqAct_PlayAgentAnimation AnimSequence;

/** Index into animationlist of current animation action */
var int AnimationIndex;

function InitBehavior(GameCrowdAgent Agent)
{
	local PlayerController PC, ClosestPC;
	local float ClosestDist, NewDist;
	local GameCrowdAgentSkeletal SkAgent;
	
	Super.InitBehavior(Agent);
	
	if ( CustomActionTarget != None )
	{
		ActionTarget = CustomActionTarget;
	}
	else if ( bLookAtPlayer )
	{
		ClosestDist = 1000000.0;
		
		// find local player, make it the action target
		foreach Agent.LocalPlayerControllers(class'PlayerController', PC)
		{
			if ( PC.Pawn != None )
			{
				NewDist = VSize(PC.Pawn.Location - Agent.Location);
				if ( NewDist < ClosestDist )
				{
					ClosestDist = NewDist;
					ClosestPC = PC;
				}
			}
		}
		if ( ClosestPC != None )
		{
			ActionTarget = ClosestPC.Pawn;
		}
	}
	
	SkAgent = GameCrowdAgentSkeletal(Agent);
	if ( SKAgent == None )
	{
		`warn("PlayAnimation behavior "$self$" called on non-skeletal agent "$Agent);
		return;
	}
	
	AnimationIndex = 0;
	
	if ( !bFaceActionTargetFirst )
	{
		PlayAgentAnimationNow();
	}
}

/**
  *  Facing target, so play animation
  */
event FinishedTargetRotation()
{
	PlayAgentAnimationNow();
}

/**
  * Set the "Out Agent" output of the current sequence to be MyAgent.
  */
native function SetSequenceOutput();

/**
  *  When an animation ends, play the next animation in the list. 
  *  If done with list, if associated with a sequence, trigger its output.
  */
event OnAnimEnd(AnimNodeSequence SeqNode, float PlayedTime, float ExcessTime)
{
	// called because we asked for early notify
	AnimationIndex++;
	if ( AnimationList.Length > AnimationIndex )
	{
		PlayAgentAnimationNow();
	}
	else
	{
		// see if there is another sequence attached to this one
		if ( (AnimSequence != None) && (AnimSequence.OutputLinks[0].Links.Length > 0) ) 
		{
			SetSequenceOutput();
			MyAgent.ClearLatentAction(class'SeqAct_PlayAgentAnimation', false);
			AnimSequence.ActivateOutputLink(0);
		}
		MyAgent.StopBehavior();
	}
}

/**
  * Play the requested animation
  */
function PlayAgentAnimationNow()
{
	local float CurrentBlendInTime, CurrentBlendOutTime;
	local GameCrowdAgentSkeletal MySkAgent;
	
	MySkAgent = GameCrowdAgentSkeletal(MyAgent);
	bFaceActionTargetFirst = false;
	MySkAgent.SetRootMotion(bUseRootMotion);
	CurrentBlendInTime = 0.0;
	CurrentBlendOutTime = 0.0;
	
	// loop if bLooping set AND marked for looping
	if ( bLooping && AnimationIndex == LoopIndex )
	{
		if ( bBlendBetweenAnims || (AnimationIndex == 0) )
		{
			CurrentBlendInTime = BlendInTime;
		}
		MySkAgent.FullBodySlot.PlayCustomAnim(AnimationList[AnimationIndex], 1.f, CurrentBlendInTime, CurrentBlendOutTime, bLooping, true); 
		if ( LoopTime > 0.0 )
		{
			MySkAgent.SetTimer(LoopTime,FALSE,nameof(OnAnimEnd));
		}
	}
	else
	{
		if ( bBlendBetweenAnims )
		{
			CurrentBlendInTime = BlendInTime;
			CurrentBlendOutTime = BlendOutTime;
		}
		else if ( AnimationIndex == 0 )
		{
			CurrentBlendInTime = BlendInTime;
		}
		MySkAgent.FullBodySlot.PlayCustomAnim(AnimationList[AnimationIndex], 1.f, CurrentBlendInTime, CurrentBlendOutTime, false, true); 
		MySkAgent.FullBodySlot.SetActorAnimEndNotification(true);
	}
	if ( AnimSequence != None )
	{
		AnimSequence.ActivateOutputLink(2);
	}
}

function StopBehavior()
{
	GameCrowdAgentSkeletal(MyAgent).FullBodySlot.StopCustomAnim(BlendOutTime);
	GameCrowdAgentSkeletal(MyAgent).SetRootMotion(FALSE);

	super.StopBehavior();
}

/** 
  * Get debug string about agent behavior
  */
function string GetBehaviorString()
{
	local string BehaviorString;
	
	BehaviorString = "Behavior: "$self;

	if ( bFaceActionTargetFirst )
	{
		BehaviorString = BehaviorString@"Turning toward "$ActionTarget;
	}
	else if ( (AnimationList.length <= AnimationIndex) || (AnimationList[AnimationIndex] == '') )
	{
		BehaviorString = BehaviorString@"MISSING ANIMATION";
	}
	else
	{
		BehaviorString = BehaviorString@"Playing "$AnimationList[AnimationIndex];
	}
	
	return BehaviorString;
}

defaultproperties
{
	bIdleBehavior=true
	AnimationIndex=0
	
	BlendInTime=0.2
	BlendOutTime=0.2
	bBlendBetweenAnims=false
	LoopTime=-1.f
}
