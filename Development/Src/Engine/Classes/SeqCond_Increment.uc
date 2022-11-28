class SeqCond_Increment extends SequenceCondition
	native(Sequence);

cpptext
{
	void Activated();
};

var() int			IncrementAmount;

/** Default value to use if no variables are linked to A */
var() int			DefaultA;

/** Default value to use if no variables are linked to B */
var() int			DefaultB;

defaultproperties
{
	ObjName="Counter"
	IncrementAmount=1

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="A <= B")
	OutputLinks(1)=(LinkDesc="A > B")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="A")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Int',LinkDesc="B")
}
