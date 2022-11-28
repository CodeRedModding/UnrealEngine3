class SeqAct_SetFloat extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
};

/** Default value to use if no variables are linked */
var() float DefaultValue;

defaultproperties
{
	ObjName="Set Float"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Target",bWriteable=true)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Value")
}
