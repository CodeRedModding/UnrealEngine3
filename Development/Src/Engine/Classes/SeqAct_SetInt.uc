class SeqAct_SetInt extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
};

/** Default value to use if no variables are linked */
var() int DefaultValue;

defaultproperties
{
	ObjName="Set Int"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="Target",bWriteable=true)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Int',LinkDesc="Value")
}
