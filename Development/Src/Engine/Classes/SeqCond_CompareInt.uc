class SeqCond_CompareInt extends SequenceCondition
	native(Sequence);

cpptext
{
	void Activated();
};

var() int DefaultA;

var() int DefaultB;

defaultproperties
{
	ObjName="Compare Int"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="A <= B")
	OutputLinks(1)=(LinkDesc="A > B")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="A")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Int',LinkDesc="B")
}
