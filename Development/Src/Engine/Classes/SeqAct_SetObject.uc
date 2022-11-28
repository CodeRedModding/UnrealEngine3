class SeqAct_SetObject extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
};

/** Default value to use if no variables are linked */
var() Object DefaultValue;

defaultproperties
{
	ObjName="Set Object"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Target",bWriteable=true)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Value")
}
