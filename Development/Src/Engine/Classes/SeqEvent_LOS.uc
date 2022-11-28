class SeqEvent_LOS extends SequenceEvent;

/** Distance from the screen center before activating this event */
var() float ScreenCenterDistance;

/** Distance from the trigger before activating this event */
var() float TriggerDistance;

/** Force a clear line of sight to the trigger? */
var() bool bCheckForObstructions;

defaultproperties
{
	ObjName="Line Of Sight"

	ScreenCenterDistance=50.f
	TriggerDistance=2048.f
	bCheckForObstructions=true
}
