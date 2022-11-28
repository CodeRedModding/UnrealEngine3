class InterpTrackDirector extends InterpTrack
	native(Interpolation);

/** 
 * InterpTrackDirector
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * A track type used for binding the view of a Player (attached to this tracks group) to the actor of a different group.
 *
 */

cpptext
{
	// InterpTrack interface
	virtual INT GetNumKeyframes();
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime);
	virtual FLOAT GetKeyframeTime(INT KeyIndex);
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst);
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true);
	virtual void RemoveKeyframe(INT KeyIndex);
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime);

	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump=false);

	virtual void DrawTrack(FRenderInterface* RI, INT TrackIndex, INT TrackWidth, INT TrackHeight, FLOAT StartTime, FLOAT PixelsPerSec, TArray<class FInterpEdSelKey>& SelectedKeys);

	// InterpTrackDirector interface
	FName GetViewedGroupName(FLOAT CurrentTime);
}

/** Information for one cut in this track. */
struct native DirectorTrackCut
{
	/** Time to perform the cut. */
	var		float	Time;

	/** GroupName of InterpGroup to cut viewpoint to. */
	var()	name	TargetCamGroup;
};	

/** Array of cuts between cameras. */
var	array<DirectorTrackCut>	CutTrack;

defaultproperties
{
	bOnePerGroup=true
	bDirGroupOnly=true
	TrackInstClass=class'Engine.InterpTrackInstDirector'
	TrackTitle="Director"
}