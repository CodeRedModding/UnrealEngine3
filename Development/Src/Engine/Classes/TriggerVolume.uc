class TriggerVolume extends Volume;

defaultproperties
{
	bColored=true
	BrushColor=(R=225,G=196,B=255,A=255)

	bCollideActors=true
	SupportedEvents.Empty
	SupportedEvents(0)=class'SeqEvent_Touch'
	SupportedEvents(1)=class'SeqEvent_UnTouch'
}
