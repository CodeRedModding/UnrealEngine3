/**
 * Activated once a sequence is activated by another operation.
 */
class SeqEvent_SequenceActivated extends SequenceEvent
	native(Sequence);

cpptext
{
	virtual void OnCreated();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
}

/** Text label to use on the sequence input link */
var() string InputLabel;

defaultproperties
{
	ObjName="Sequence Activated"
	InputLabel="Default Input"
	bPlayerOnly=false
}
