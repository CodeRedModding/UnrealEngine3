/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdAgentSkeletal extends GameCrowdAgent
	native
	abstract;

/** SkeletalMeshComponent used for crowd member mesh */
var(Rendering)	SkeletalMeshComponent			SkeletalMeshComponent;

/** Cached pointer to speed blend node */
var		AnimNodeBlend					SpeedBlendNode;

/** Cached pointer to action blend node */
var		AnimNodeSlot					FullBodySlot;

/** Cached pointer to action animation player */
var		AnimNodeSequence				ActionSeqNode;

/** Cached pointer to walking animation player */
var		AnimNodeSequence				WalkSeqNode;

/** Cached pointer to running animation player */
var		AnimNodeSequence				RunSeqNode;

/** Cached pointer to AnimTree instance (SkeletalMeshComponent.Animations) */
var		AnimTree						AgentTree;

/** The names of the animation loops to use when moving slowly */
var(Rendering)	array<name>		WalkAnimNames;

/** The name of the animations to use when moving more quickly */
var(Rendering)	array<name>		RunAnimNames;

/** The name of the animations to use when not moving (and not playing a custom animation) */
var(Rendering)	array<name>		IdleAnimNames;

/** Set of possible animation names to play when agent dies */
var(Behavior)		array<name>				DeathAnimNames;

/** Below this speed, the walking animation is used (if the AnimTree has a SpeedBlendNode, and not using root motion) */
var(SpeedBlendAnim)	float	SpeedBlendStart;

/** Above this speed, the running animation is used. Between this and SpeedBlendStart the animations are blended (if the AnimTree has a SpeedBlendNode, and not using root motion)*/
var(SpeedBlendAnim)	float	SpeedBlendEnd;

/** This controls how the animation playback rate changes based on the speed of the agent (if not using root motion) */
var(SpeedBlendAnim)	float	AnimVelRate;

/** Limits how quickly blending between running and walking can happen. (if not using root motion) */
var(SpeedBlendAnim)	float	MaxSpeedBlendChangeSpeed;

/** Name of sync group for movement, whose rate is scaled  (if not using root motion) */
var(SpeedBlendAnim)	name	MoveSyncGroupName;

/** Info about mesh we might want to use as an attachment. */
struct native GameCrowdAttachmentInfo
{
	/** Pointer to mesh to attach */
	var()	StaticMesh		StaticMesh;
	/** Chance of choosing this attachment. */
	var()	float			Chance;
	/** Scaling applied to mesh when attached */
	var()	vector			Scale3D;

	structdefaultproperties
	{
		Chance=1.0
		Scale3D=(X=1.0,Y=1.0,Z=1.0)
	}
};

/** Info about things you can attach to one socket. */
struct native GameCrowdAttachmentList
{
	/** Name of socket to attach mesh to */
	var()	name SocketName;
	/** List of possible meshes to attach to this socket. */
	var()	array<GameCrowdAttachmentInfo>	List;
};

/** List of sets of meshes to attach to agent.  */
var(Rendering) array<GameCrowdAttachmentList>	Attachments;

/** Maximum time to try to rotate toward a target before playing animation */
var(Behavior) float MaxTargetAcquireTime;

/** If true, clamp velocity based on root motion in movement animations */
var(Rendering) bool bUseRootMotionVelocity; 

/** If true, then allow enabling/disable of the skeleton based on whether the crowd agent is ticking or not */
var(Rendering) bool bAllowSkeletonUpdateChangeBasedOnTickResult;

/** If true, always tick when not visible */
var(Tick) bool bTickWhenNotVisible;

/** If the crowd agent isn't visible for this length of time (in seconds), then stop ticking */
var(Tick) float NotVisibleDisableTickTime;

/** true if currently playing idle animation */
var bool bIsPlayingIdleAnimation;

/** true if currently playing death animation */
var bool bIsPlayingDeathAnimation;

/** If true, then the crowd agent is currently playing an important animation and should never try to disable the skeleton */
var bool bIsPlayingImportantAnimation;

/** Whether to perform animation updates this tick on this agent ( updated using ShouldPerformCrowdSimulation() )*/
var bool bAnimateThisTick;

/** Maximum distance from camera at which this agent should be animated */
var(LOD) float MaxAnimationDistance;

/** Keep square of MaxAnimationDistance for faster testing */
var float MaxAnimationDistanceSq;

cpptext
{
	virtual void TickSpecial( FLOAT DeltaSeconds );
	virtual void UpdatePendingVelocity( FLOAT DeltaTime );
}

simulated function PostBeginPlay()
{
	super.PostBeginPlay();
	
	if ( bDeleteMe )
	{
		return;
	}

	// Cache pointers to anim nodes
	SpeedBlendNode = AnimNodeBlend(SkeletalMeshComponent.FindAnimNode('SpeedBlendNode'));
	FullBodySlot = AnimNodeSlot(SkeletalMeshComponent.FindAnimNode('ActionBlendNode'));
	ActionSeqNode = AnimNodeSequence(SkeletalMeshComponent.FindAnimNode('ActionSeqNode'));
	WalkSeqNode = AnimNodeSequence(SkeletalMeshComponent.FindAnimNode('WalkSeqNode'));
	RunSeqNode = AnimNodeSequence(SkeletalMeshComponent.FindAnimNode('RunSeqNode'));
	AgentTree = AnimTree(SkeletalMeshComponent.Animations);

	// Assign random walk/run cycle
	if( (WalkSeqNode != None) && (WalkAnimNames.Length > 0) )
	{
		WalkSeqNode.SetAnim(WalkAnimNames[Rand(WalkAnimNames.length)]);
	}

	if( (RunSeqNode != None) && (RunAnimNames.Length > 0) )
	{
		RunSeqNode.SetAnim(RunAnimNames[Rand(RunAnimNames.length)]);
	}

	if( ActionSeqNode != None )
	{
		ActionSeqNode.bZeroRootTranslation = TRUE;
	}
	
	if ( bUseRootMotionVelocity )
	{
		SkeletalMeshComponent.RootMotionMode = RMM_Accel;
		WalkSeqNode.SetRootBoneAxisOption(RBA_Translate, RBA_Translate, RBA_Translate);
		RunSeqNode.SetRootBoneAxisOption(RBA_Translate, RBA_Translate, RBA_Translate);
	}

	MaxAnimationDistanceSq = MaxAnimationDistance * MaxAnimationDistance;
}

simulated function SetLighting(bool bEnableLightEnvironment, LightingChannelContainer AgentLightingChannel, bool bCastShadows)
{
	Super.SetLighting(bEnableLightEnvironment, AgentLightingChannel, bCastShadows);
	
	SkeletalMeshComponent.SetLightingChannels(AgentLightingChannel);

	// Do attachments
	CreateAttachments();
	
	SkeletalMeshComponent.CastShadow = bCastShadows;
	SkeletalMeshComponent.bCastDynamicShadow = bCastShadows;
	SkeletalMeshComponent.ForceUpdate(FALSE);
/*		if ( bEnableLightEnvironment )
	{
		LightEnvironment.bCastShadows = true;
	}*/
}

/** Stop agent moving and play death anim */
native function PlayDeath(vector KillMomentum);

/** 
  *  Enable or disable root motion for this agent
  */
native function SetRootMotion(bool bRootMotionEnabled);

/** 
  *  Animation request from kismet
  */
simulated function OnPlayAgentAnimation(SeqAct_PlayAgentAnimation Action)
{
	if ( Action.InputLinks[1].bHasImpulse )
	{
		Action.ActivateOutputLink(1);
		// stop playing animations defined by action
		StopBehavior(); // FIXMESTEVE - check matchup with action?
		if ( CurrentDestination.ReachedByAgent(self, Location, false) )
		{
			CurrentDestination.ReachedDestination(self);
		}
	}
	else
	{
		// play animations defined by action
		Action.SetCurrentAnimationActionFor(self);
	}
}

event ClearLatentAnimation()
{
	ClearLatentAction(class'SeqAct_PlayAgentAnimation', false);
}

/**
  * Play a looping idle animation
  */
simulated event PlayIdleAnimation()
{
	bIsPlayingIdleAnimation = true;
	FullBodySlot.PlayCustomAnim(IdleAnimNames[Rand(IdleAnimNames.length)], 1.0, 0.1, 0.1, true, false); 
}

simulated event StopIdleAnimation()
{
	FullBodySlot.StopCustomAnim(0.1);
	bIsPlayingIdleAnimation = false;
}

event OnAnimEnd(AnimNodeSequence SeqNode, float PlayedTime, float ExcessTime)
{
	// called because we asked for early notify
	if ( CurrentBehavior != None )
	{
		CurrentBehavior.OnAnimEnd(SeqNode, PlayedTime, ExcessTime);
	}
}	
	

/** Create any attachments */
simulated function CreateAttachments()
{
	local int		AttachIdx, InfoIdx, PickedInfoIdx;
	local float		ChanceTotal, RandVal;
	local StaticMeshComponent	StaticMeshComp;
	local bool		bUseSocket, bUseBone;

	// Iterate over each list/attachment point.
	for(AttachIdx=0; AttachIdx < Attachments.length; AttachIdx++ )
	{
		// Skip over empty lists
		if(Attachments[AttachIdx].List.length == 0)
		{
			continue;
		}

		// We need to choose one from he list, using the 'Chance' values.
		// First we need to total of all of them
		ChanceTotal = 0.0;
		for(InfoIdx=0; InfoIdx < Attachments[AttachIdx].List.length; InfoIdx++)
		{
			ChanceTotal += Attachments[AttachIdx].List[InfoIdx].Chance;
		}
		// Now pick a value between 0.0 and ChanceTotal
		RandVal = FRand() * ChanceTotal;

		// Now go over list again - when we pass RandVal, that is our attachment
		ChanceTotal = 0.0;
		for(InfoIdx=0; InfoIdx < Attachments[AttachIdx].List.length; InfoIdx++)
		{
			ChanceTotal += Attachments[AttachIdx].List[InfoIdx].Chance;
			if(ChanceTotal >= RandVal)
			{
				PickedInfoIdx = InfoIdx;
				break;
			}
		}

		// Ok, so now we know what we want to attach.
		if( Attachments[AttachIdx].List[PickedInfoIdx].StaticMesh != None )
		{
			// See if name is a socket or a bone (if both, favours socket)
			bUseSocket = (SkeletalMeshComponent.GetSocketByName(Attachments[AttachIdx].SocketName) != None);
			bUseBone = (SkeletalMeshComponent.MatchRefBone(Attachments[AttachIdx].SocketName) != INDEX_NONE);

			// See if we found valid attachment point
			if(bUseSocket || bUseBone)
			{
				// Actually create the StaticMeshComponent
				StaticMeshComp = new(self) class'StaticMeshComponent';
				StaticMeshComp.SetStaticMesh( Attachments[AttachIdx].List[PickedInfoIdx].StaticMesh );
				StaticMeshComp.SetActorCollision(FALSE, FALSE);
				StaticMeshComp.SetScale3D( Attachments[AttachIdx].List[PickedInfoIdx].Scale3D );
				StaticMeshComp.SetLightEnvironment(LightEnvironment);

				// Attach it to socket or bone
				if(bUseSocket)
				{
					SkeletalMeshComponent.AttachComponentToSocket(StaticMeshComp, Attachments[AttachIdx].SocketName);
				}
				else
				{
					SkeletalMeshComponent.AttachComponent(StaticMeshComp, Attachments[AttachIdx].SocketName);		
				}
			}
			else
			{
				`log("CrowdAgent: WARNING: Could not find socket or bone called '"$Attachments[AttachIdx].SocketName$"' for mesh '"@Attachments[AttachIdx].List[PickedInfoIdx].StaticMesh$"'");
			}
		}
	}
}


defaultproperties
{
	SpeedBlendStart=150.0
	SpeedBlendEnd=180.0

	AnimVelRate=0.0098
	MaxSpeedBlendChangeSpeed=2.0
	MoveSyncGroupName=MoveGroup
	MaxTargetAcquireTime=5.0

	MaxAnimationDistance=12000.0

	NotVisibleDisableTickTime=0.2
	bAllowSkeletonUpdateChangeBasedOnTickResult=true

	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		CollideActors=true
		bEnableLineCheckWithBounds=TRUE
		BlockActors=false
		BlockZeroExtent=true
		BlockNonZeroExtent=false
		BlockRigidBody=false
		LightEnvironment=MyLightEnvironment
		RBChannel=RBCC_GameplayPhysics
		bCastDynamicShadow=FALSE
		RBCollideWithChannels=(Default=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE)
		bUpdateSkelWhenNotRendered=FALSE
		bAcceptsDynamicDecals=FALSE // for crowds there are so many of them probably not going to notice not getting decals on them.  Each decal on them causes entire SkelMesh to be rerendered
		bTickAnimNodesWhenNotRendered=FALSE
	End Object
	SkeletalMeshComponent=SkeletalMeshComponent0
	Components.Add(SkeletalMeshComponent0)
}
