class SeqAct_Possess extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

var transient Pawn	PawnToPossess;

defaultproperties
{
	ObjName="Possess Pawn"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Pawn Target")
}
