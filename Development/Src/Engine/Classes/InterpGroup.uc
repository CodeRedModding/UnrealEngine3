class InterpGroup extends Object
	native(Interpolation)
	collapsecategories
	inherits(FInterpEdInputInterface)
	hidecategories(Object);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * A group, associated with a particular Actor or set of Actors, which contains a set of InterpTracks for interpolating 
 * properties of the Actor over time.
 * The Outer of an InterpGroup is an InterpData.
 */

cpptext
{
	// UObject interface
	virtual void PostLoad();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UInterpGroup interface

	/** Iterate over all InterpTracks in this InterpGroup, doing any actions to bring the state to the specified time. */
	virtual void UpdateGroup(FLOAT NewPosition, class UInterpGroupInst* GrInst, UBOOL bPreview, UBOOL bJump);

	/** Ensure this group name is unique within this InterpData (its Outer). */
	void EnsureUniqueName();

	/** 
	 *	Find all the tracks in this group of a specific class.
	 *	Tracks are in the output array in the order they appear in the group.
	 */
	void FindTracksByClass(UClass* TrackClass, TArray<class UInterpTrack*>& OutputTracks);

	/** Returns whether this Group contains at least one AnimControl track. */
	UBOOL HasAnimControlTrack() const;

	/** Returns whether this Group contains a movement track. */
	UBOOL HasMoveTrack() const;

	/** Iterate over AnimControl tracks in this Group, build the anim blend info structures, and pass to the Actor via (Preview)SetAnimWeights. */
	void UpdateAnimWeights(FLOAT NewPosition, class UInterpGroupInst* GrInst, UBOOL bPreview, UBOOL bJump);

	/** Util for determining how many AnimControl tracks within this group are using the Slot with the supplied name. */
	INT GetAnimTracksUsingSlot(FName InSlotName);
}

struct InterpEdSelKey
{
	var InterpGroup Group;
	var InterpTrack	Track;
	var int			KeyIndex;
	var float		UnsnappedPosition;
};


var		export array<InterpTrack>		InterpTracks;

/** 
 *	Within an InterpData, all GroupNames must be unique. 
 *	Used for naming Variable connectors on the Action in Kismet and finding each groups object.
 */
var		name					GroupName; 

/** Colour used for drawing tracks etc. related to this group. */
var()	color					GroupColor;

/** 
 *	The AnimSets that are used by any AnimControl tracks. 
 *	These will be passed to the Actor when the cinematic begins, and sequences named in the tracks should be found in them. 
 */
var()	array<AnimSet>			GroupAnimSets;

/** Whether or not this group is folded away in the editor. */
var		bool					bCollapsed;

/** Whether or not this group is visible in the editor. */
var		transient	bool		bVisible;

/** When enabled, this group is treated like a folder in the editor, which should only be used for organization.  Folders are never associated with actors and don't have a presence in the Kismet graph. */
var		bool					bIsFolder;

/** When true, this group is considered a 'visual child' of another group.  This doesn't at all affect the behavior of the group, it's only for visual organization.  Also, it's implied that the parent is the next prior group in the array that doesn't have a parent. */
var		bool					bIsParented;

/** When enabled, this group will be selected in the interp editor. */
var		transient	bool		bIsSelected;

defaultproperties
{
	GroupName="InterpGroup"
	GroupColor=(R=100,G=80,B=200,A=255)
	bVisible=true
	bIsSelected=false
}
