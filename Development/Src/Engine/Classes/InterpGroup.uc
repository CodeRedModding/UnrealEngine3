class InterpGroup extends Object
	native(Interpolation)
	collapsecategories
	hidecategories(Object);

/** 
 * InterpGroup
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * A group, associated with a particular Actor or set of Actors, which contains a set of InterpTracks for interpolating 
 * properties of the Actor over time.
 * The Outer of an InterpGroup is an InterpData.
 */

cpptext
{
	// UInterpGroup interface

	/** Iterate over all InterpTracks in this InterpGroup, doing any actions to bring the state to the specified time. */
	virtual void UpdateGroup(FLOAT NewPosition, class UInterpGroupInst* GrInst, UBOOL bPreview=false, UBOOL bJump=false);

	/** Ensure this group name is unique within this InterpData (its Outer). */
	void EnsureUniqueName();

	void FindTracksByClass(UClass* TrackClass, TArray<class UInterpTrack*>& OutputTracks);
}

struct InterpEdSelKey
{
	var InterpGroup Group;
	var int			TrackIndex;
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

/** Whether or not this group is folded away in the editor. */
var		bool					bCollapsed;

defaultproperties
{
	GroupName="InterpGroup"
	GroupColor=(R=100,G=80,B=200,A=255)
}