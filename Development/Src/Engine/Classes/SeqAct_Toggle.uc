class SeqAct_Toggle extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

defaultproperties
{
	ObjName="Toggle"

	InputLinks(0)=(LinkDesc="Turn On")
	InputLinks(1)=(LinkDesc="Turn Off")
	InputLinks(2)=(LinkDesc="Toggle")

    VariableLinks(1)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Bool",MinVars=0)
	EventLinks(0)=(LinkDesc="Event")
}
