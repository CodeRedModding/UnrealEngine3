/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class SeqAct_PlayAgentAnimation extends SeqAct_Latent
	dependson(GameCrowdAgent)
	native;

/** List of animations to play while at this node */
var() Array<name> AnimationList;

var() float BlendInTime;
var() float BlendOutTime;

var() bool bUseRootMotion;

/** If true, face action target before starting animation */
var() bool bFaceActionTargetFirst;

/** If true, loop the last animation in the list forever */
var() bool bLooping;

/** Which animation to loop in AnimationList if bLooping == TRUE */
var() int LoopIndex;

/** How long to loop the animation if bLooping == TRUE, -1.f == infinite */
var() float LoopTime;

/** Whether should blend between animations in the list.  Set True if they don't match at start/end */
var() bool bBlendBetweenAnims;


/** Optional other actor that actions should point at, instead of at the actual destination location. */
var actor ActionTarget;

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

function SetCurrentAnimationActionFor(GameCrowdAgentSkeletal Agent)
{
	local GameCrowdBehavior_PlayAnimation AnimBehavior;
	local int i;
	
	// init behavior properties
	AnimBehavior = new(Agent) class'GameCrowdBehavior_PlayAnimation';
	AnimBehavior.AnimSequence = self;
	AnimBehavior.BlendInTime = BlendInTime;
	AnimBehavior.BlendOutTime = BlendOutTime;
	AnimBehavior.bUseRootMotion = bUseRootMotion;
	AnimBehavior.bFaceActionTargetFirst = bFaceActionTargetFirst;
	AnimBehavior.bLooping = bLooping;
	AnimBehavior.LoopIndex = LoopIndex;
	AnimBehavior.LoopTime = LoopTime;
	AnimBehavior.bBlendBetweenAnims = bBlendBetweenAnims;
	AnimBehavior.CustomActionTarget = ActionTarget;
	
	for ( i=0; i<AnimationList.Length; i++ )
	{
		AnimBehavior.AnimationList[i] = AnimationList[i];
	}

	// activate behavior
	Agent.ActivateInstancedBehavior(AnimBehavior);
}

cpptext
{
	virtual void Activated();
	virtual UBOOL UpdateOp(FLOAT DeltaTime);
}

defaultproperties
{
	ObjName="Play Agent Animation"
	ObjCategory="Crowd"

	InputLinks(0)=(LinkDesc="Play")
	InputLinks(1)=(LinkDesc="Stop")

	OutputLinks(0)=(LinkDesc="Finished")
	OutputLinks(1)=(LinkDesc="Stopped")
	OutputLinks(2)=(LinkDesc="Started")
	
	bAutoActivateOutputLinks=false
	
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Action Focus",PropertyName=ActionTarget)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Out Agent",bWriteable=true)

	BlendInTime=0.2
	BlendOutTime=0.2
	bBlendBetweenAnims=false

	LoopTime=-1.f
}
