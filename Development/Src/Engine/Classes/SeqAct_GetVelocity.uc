class SeqAct_GetVelocity extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
}

defaultproperties
{
	ObjName="Get Velocity"
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Velocity",bWriteable=true)
}
