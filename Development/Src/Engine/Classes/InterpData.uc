class InterpData extends SequenceVariable
	native(Sequence);

/** 
 * InterpData
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * 
 * Actual interpolation data, containing keyframe tracks, event tracks etc.
 * This does not contain any Actor references or state, so can safely be stored in 
 * packages, shared between multiple Actors/SeqAct_Interps etc.
 */

/** Duration of interpolation sequence - in seconds. */
var float				InterpLength; 

/** Position in Interp to move things to for path-building in editor. */
var float				PathBuildTime;

/** Actual interpolation data. Groups of InterpTracks. */
var	export array<InterpGroup>	InterpGroups;

/** Used for curve editor to remember curve-editing setup. Only loaded in editor. */
var	export InterpCurveEdSetup	CurveEdSetup;

/** Used in editor for defining sections to loop, stretch etc. */
var	float				EdSectionStart;

/** Used in editor for defining sections to loop, stretch etc. */
var	float				EdSectionEnd;

cpptext
{
	// SequenceVariable interface
	virtual FString GetValueStr();

	// InterpData interface
	INT FindGroupByName(FName GroupName);
	void FindTracksByClass(UClass* TrackClass, TArray<class UInterpTrack*>& OutputTracks);
	class UInterpGroupDirector* FindDirectorGroup();
	void GetAllEventNames(TArray<FName>& OutEventNames);
}

defaultproperties
{
	InterpLength=5.0
	EdSectionStart=1.0
	EdSectionEnd=2.0
	PathBuildTime=0.0

	ObjName="Matinee Data"
	ObjColor=(R=255,G=128,B=0,A=255)
}