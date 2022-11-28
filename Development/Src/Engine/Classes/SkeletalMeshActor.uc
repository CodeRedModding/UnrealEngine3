class SkeletalMeshActor extends Actor
	native
	placeable;

var() SkeletalMeshComponent			SkeletalMeshComponent;

/** Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle action)
{
	local AnimNodeSequence SeqNode;

	SeqNode = AnimNodeSequence(SkeletalMeshComponent.Animations);

	// Turn ON
	if (action.InputLinks[0].bHasImpulse)
	{
		// If animation is not playing - start playing it now.
		if(!SeqNode.bPlaying)
		{
			// This starts the animation playing from the beginning. Do we always want that?
			SeqNode.PlayAnim(SeqNode.bLooping, SeqNode.Rate, 0.0);
		}
	}
	// Turn OFF
	else if (action.InputLinks[1].bHasImpulse)
	{
		// If animation is playing, stop it now.
		if(SeqNode.bPlaying)
		{
			SeqNode.StopAnim();
		}
	}
	// Toggle
	else if (action.InputLinks[2].bHasImpulse)
	{
		// Toggle current animation state.
		if(SeqNode.bPlaying)
		{
			SeqNode.StopAnim();
		}
		else
		{
			SeqNode.PlayAnim(SeqNode.bLooping, SeqNode.Rate, 0.0);
		}
	}
}

defaultproperties
{
	Begin Object Class=AnimNodeSequence Name=AnimNodeSeq0
	End Object

	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		Animations=AnimNodeSeq0
		CollideActors=true
		BlockActors=false
		BlockZeroExtent=true
		BlockNonZeroExtent=false
		BlockRigidBody=false
	End Object
	SkeletalMeshComponent=SkeletalMeshComponent0
	CollisionComponent=SkeletalMeshComponent0
	Components.Remove(CollisionCylinder)
	Components.Remove(Sprite)
	Components.Add(SkeletalMeshComponent0)

	Physics=PHYS_None
	bEdShouldSnap=true
	bStatic=false
	bCollideActors=true
	bBlockActors=false
	bWorldGeometry=false
	bCollideWorld=false
}