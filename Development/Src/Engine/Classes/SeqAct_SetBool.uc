class SeqAct_SetBool extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

var() bool DefaultValue;

defaultproperties
{
	ObjName="Set Bool"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Target",bWriteable=true)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Value",MinVars=0)
}
