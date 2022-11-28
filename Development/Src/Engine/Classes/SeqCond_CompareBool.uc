class SeqCond_CompareBool extends SequenceCondition
	native(Sequence);

cpptext
{
	void Activated();
};

defaultproperties
{
	ObjName="Compare Bool"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="True")
	OutputLinks(1)=(LinkDesc="False")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Bool")
}
