class InterpTrack extends Object
	native
	noexport
	collapsecategories
	hidecategories(Object)
	abstract;

/** 
 * InterpTrack
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * Abstract base class for a track of interpolated data. Contains the actual data.
 * The Outer of an InterpTrack is the InterpGroup.
 */

/** FCurveEdInterface virtual function table. */
var private native noexport pointer	CurveEdVTable;

/** Subclass of InterpTrackInst that should be created when instancing this track. */
var	class<InterpTrackInst>	TrackInstClass;

/** Title of track type. */
var		string	TrackTitle;

/** Whether there may only be one of this track in an InterpGroup. */
var		bool	bOnePerGroup; 

/** If this track can only exist inside the Director group. */
var		bool	bDirGroupOnly;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInst'

	TrackTitle="Track"
}