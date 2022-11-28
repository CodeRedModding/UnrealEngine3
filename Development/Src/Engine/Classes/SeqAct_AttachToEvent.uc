class SeqAct_AttachToEvent extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

defaultproperties
{
	ObjName="Attach To Event"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Attachee")
	EventLinks(0)=(LinkDesc="Event")
}
