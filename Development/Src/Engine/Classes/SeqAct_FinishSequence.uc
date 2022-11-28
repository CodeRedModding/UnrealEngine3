/**
 * Upon activation this action triggers the associated output link
 * of the owning Sequence.
 */
class SeqAct_FinishSequence extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
	virtual void OnCreated();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
}

/** Text label to use on the sequence output link */
var() string OutputLabel;

defaultproperties
{
	ObjName="Finish Sequence"
	OutputLabel="Default Output"
	OutputLinks.Empty
	VariableLinks.Empty
}
