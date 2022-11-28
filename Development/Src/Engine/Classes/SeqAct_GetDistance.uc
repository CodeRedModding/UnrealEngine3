class SeqAct_GetDistance extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
}

defaultproperties
{
	ObjName="Get Distance"
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="A")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="B")
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Distance",bWriteable=true)
}
