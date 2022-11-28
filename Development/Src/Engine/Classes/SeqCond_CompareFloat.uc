class SeqCond_CompareFloat extends SequenceCondition
	native(Sequence);

cpptext
{
	void Activated();
};

var() float DefaultA;

var() float DefaultB;

defaultproperties
{
	ObjName="Compare Float"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="A <= B")
	OutputLinks(1)=(LinkDesc="A > B")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="A")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="B")
}
