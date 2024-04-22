/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Abstract base class for a track of interpolated data. Contains the actual data.
 * The Outer of an InterpTrack is the InterpGroup.
 */

class InterpTrack extends Object
	native
	noexport
	collapsecategories
	hidecategories(Object)
	inherits(FInterpEdInputInterface)
	abstract;


/** FCurveEdInterface virtual function table. */
var private native noexport pointer	CurveEdVTable;

/** Helper struct for creating sub tracks supported by this track */
struct native SupportedSubTrackInfo
{
	/** The sub track class which is supported by this track */
	var class<InterpTrack> SupportedClass;
	/** The name of the subtrack */
	var String SubTrackName;
	/** Index into the any subtrack group this subtrack belongs to (can be -1 for no group) */
	var int GroupIndex;
};

/** A small structure holding data for grouping subtracks. (For UI drawing purposes) */
struct native SubTrackGroup
{
	/** Name of the subtrack  group */
	var string	GroupName;
	/** Indices to tracks in the parent track subtrack array. */
	var array<int> TrackIndices;
	/** If this group is collapsed */
	var bool	bIsCollapsed;
	/** If this group is selected */ 
	var transient bool	bIsSelected;
};

/** A list of subtracks that belong to this track */
var array<InterpTrack> SubTracks;

/** A list of subtrack groups (for editor UI organization only) */
var editoronly array<SubTrackGroup> SubTrackGroups;

/** A list of supported tracks that can be a subtrack of this track. */
var transient editoronly array< SupportedSubTrackInfo > SupportedSubTracks;

var	class<InterpTrackInst>	TrackInstClass;

/** Required condition for this track to be enabled */
enum ETrackActiveCondition
{
	/** Track is always active */
	ETAC_Always,

	/** Track is active when extreme content (gore) is enabled */
	ETAC_GoreEnabled,

	/** Track is active when extreme content (gore) is disabled */
	ETAC_GoreDisabled
};


/** Sets the condition that must be met for this track to be enabled */
var() ETrackActiveCondition ActiveCondition;

/** Title of track type. */
var		string	TrackTitle;

/** Whether there may only be one of this track in an InterpGroup. */
var		bool	bOnePerGroup; 

/** If this track can only exist inside the Director group. */
var		bool	bDirGroupOnly;

/** Whether or not this track should actually update the target actor. */
var		bool	bDisableTrack;

/** If true, the Actor this track is working on will have BeginAnimControl/FinishAnimControl called on it. */
var		bool	bIsAnimControlTrack;

/** If this track can only exist as a sub track. */
var		bool	bSubTrackOnly;

/** Whether or not this track is visible in the editor. */
var		transient	bool		bVisible;

/** Whether or not this track is selected in the editor. */
var		transient	bool		bIsSelected;

/** Whether or not this track is recording in the editor. */
var		transient	bool		bIsRecording;

/** If this track is collapsed. (Only applies  to tracks with subtracks). */
var		editoronly	bool		bIsCollapsed;


defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInst'

	ActiveCondition=ETAC_Always
	TrackTitle="Track"
	bVisible=true
	bIsSelected=false
	bIsRecording=false
	bIsCollapsed=false

}
