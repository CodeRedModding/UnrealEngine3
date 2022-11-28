class SeqAct_MoveToActor extends SequenceAction;

/** Should this move be interruptable? */
var() bool bInterruptable;

defaultproperties
{
	ObjName="Move To Actor"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Destination")
}
