
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Animation Node used to synch childs.
 * Would be typically used to synch several walk/run/crouch cycles together.
 *
 * This node works by using the most relevant node in the final blend as the master node,
 * to update all the others (slaves).
 * This requires all cycles to be relatively synched (i.e. left foot is down on all cycles at 0.25% of the animation, regarless of its length).
 */

class AnimNodeSynch extends AnimNodeBlendBase
	native(Anim);

/** definition of a group of AnimNodeSequence to synchronize together */
struct native SynchGroup
{
	/** Cached array of anim node sequence nodes to synchronize */
	var				Array<AnimNodeSequence>	SeqNodes;
	/** Last master node used, do not search for a new one, if this one has a full weight... */
	var	transient	AnimNodeSequence		MasterNode;
	/** Name of group. */
	var()			Name					GroupName;
	/** If FALSE, do not trigger slave nodes notifies. */
	var()			bool					bFireSlaveNotifies;
	/** Rate Scale */
	var()			float					RateScale;

	structdefaultproperties
	{
		RateScale=1.f
	}
};


/** List of groups to synchronize */
var()	Array<SynchGroup>	Groups;

cpptext
{
	virtual void	InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent);
	virtual	void	TickAnim(FLOAT DeltaSeconds);

	void			UpdateMasterNodeForGroup(FSynchGroup& SynchGroup);
	void			RepopulateGroups();
}
	

/** Add a node to an existing group */
native final function AddNodeToGroup(AnimNodeSequence SeqNode, Name GroupName);

/** Remove a node from an existing group */
native final function RemoveNodeFromGroup(AnimNodeSequence SeqNode, Name GroupName);

/** Accesses the Master Node driving a given group */
native final function AnimNodeSequence GetMasterNodeOfGroup(Name GroupName);

/** Force a group at a relative position. */
native final function ForceRelativePosition(Name GroupName, FLOAT RelativePosition);

/** Get the relative position of a group. */
native final function float GetRelativePosition(Name GroupName);

/** Adjust the Rate Scale of a group */
native final function SetGroupRateScale(Name GroupName, FLOAT NewRateScale);

defaultproperties
{
	Children(0)=(Name="Input",Weight=1.0)
	bFixNumChildren=TRUE
}
