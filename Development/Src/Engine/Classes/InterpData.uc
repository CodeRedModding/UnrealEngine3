/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * InterpData
 *
 *
 * Actual interpolation data, containing keyframe tracks, event tracks etc.
 * This does not contain any Actor references or state, so can safely be stored in
 * packages, shared between multiple Actors/SeqAct_Interps etc.
 */

class InterpData extends SequenceVariable
	native(Sequence);



/** Duration of interpolation sequence - in seconds. */
var float				InterpLength;

/** Position in Interp to move things to for path-building in editor. */
var float				PathBuildTime;

/** Actual interpolation data. Groups of InterpTracks. */
var	export array<InterpGroup>	InterpGroups;

/** Used for curve editor to remember curve-editing setup. Only loaded in editor. */
var	export InterpCurveEdSetup	CurveEdSetup;

/** Used for filtering which tracks are currently visible. */
var editoronly array<InterpFilter>	InterpFilters;

/** The currently selected filter. */
var editoronly InterpFilter			SelectedFilter;

/** Array of default filters. */
var editoronly transient array<InterpFilter> DefaultFilters;

/** Used in editor for defining sections to loop, stretch etc. */
var	float				EdSectionStart;

/** Used in editor for defining sections to loop, stretch etc. */
var	float				EdSectionEnd;

/** If TRUE, then the matinee should be baked and pruned at cook time. */
var() bool bShouldBakeAndPrune;

struct native AnimSetBakeAndPruneStatus
{
	/** Name of the anim set referenced in Matinee */
	var() editconst string	AnimSetName;
	/** If TRUE, the animation is in a GroupAnimSets array, but is unused */
	var() editconst	bool	bReferencedButUnused;
	/** If TRUE, skip BakeAndPrune on this anim set during cooking */
	var() bool				bSkipBakeAndPrune;
	/** If TRUE, skip this set entirely when cooking, as it for previewing only */
	var() bool              bSkipCooking;
};

/** AnimSets referenced by this matinee, and whether to allow bake and prune on them during cooking. */
var() editfixedsize array<AnimSetBakeAndPruneStatus> BakeAndPruneStatus;

/** Cached version of the director group, if any, for easy access while in game */
var transient InterpGroupDirector CachedDirectorGroup; 

cpptext
{
	// UObject interface
	/**
	 * This function is being called after all objects referenced by this object have been serialized.
	 */
	virtual void PostLoad(void);

	// SequenceVariable interface
	virtual FString GetValueStr();

	/** Search through all InterpGroups in this InterpData to find a group whose GroupName matches the given name. Returns INDEX_NONE if group not found. */
	INT FindGroupByName( FName GroupName );
	INT FindGroupByName( const FString& InGroupName );

	void FindTracksByClass(UClass* TrackClass, TArray<class UInterpTrack*>& OutputTracks);
	class UInterpGroupDirector* FindDirectorGroup();
	void GetAllEventNames(TArray<FName>& OutEventNames);

#if WITH_EDITOR
	void UpdateBakeAndPruneStatus();
#endif
}

defaultproperties
{
	InterpLength=5.0
	EdSectionStart=0.0
	EdSectionEnd=1.0
	PathBuildTime=0.0

	bShouldBakeAndPrune=false

	ObjName="Matinee Data"
	ObjColor=(R=255,G=128,B=0,A=255)		// orange

	Begin Object Class=InterpFilter Name=FilterAll
		Caption="All"
	End Object

	Begin Object Class=InterpFilter_Classes Name=FilterCameras
		Caption="Cameras"
		ClassToFilterBy=CameraActor
	End Object

	Begin Object Class=InterpFilter_Classes Name=FilterSkeletalMeshes
		Caption="Skeletal Meshes"
		ClassToFilterBy=SkeletalMeshActor
	End Object

	Begin Object Class=InterpFilter_Classes Name=FilterLighting
		Caption="Lights"
		ClassToFilterBy=Light
	End Object

	Begin Object Class=InterpFilter_Classes Name=FilterEmitters
		Caption="Particles"
		ClassToFilterBy=Emitter
	End Object

	Begin Object Class=InterpFilter_Classes Name=FilterSounds
		Caption="Sounds"
		TrackClasses=(InterpTrackSound)
	End Object

	Begin Object Class=InterpFilter_Classes Name=FilterEvents
		Caption="Events"
		TrackClasses=(InterpTrackEvent)
	End Object

	DefaultFilters=(FilterAll, FilterCameras, FilterSkeletalMeshes, FilterLighting, FilterEmitters, FilterSounds, FilterEvents)
}
