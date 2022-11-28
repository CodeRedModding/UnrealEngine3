class InterpTrackEvent extends InterpTrack
	native(Interpolation);

/** 
 * InterpTrackEvent
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * 
 *	A track containing discrete events that are triggered as its played back. 
 *	Events correspond to Outputs of the SeqAct_Interp in Kismet.
 *	There is no PreviewUpdateTrack function for this type - events are not triggered in editor.
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
}

/** Information for one event in the track. */
struct native EventTrackKey
{
	var		float	Time;
	var()	name	EventName;
};	

/** Array of events to fire off. */
var	array<EventTrackKey>	EventTrack;

/** If events should be fired when passed playing the sequence forwards. */
var() bool	bFireEventsWhenForwards;

/** If events should be fired when passed playing the sequence backwards. */
var() bool	bFireEventsWhenBackwards;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstEvent'
	TrackTitle="Event"
	bFireEventsWhenForwards=true
	bFireEventsWhenBackwards=true
}