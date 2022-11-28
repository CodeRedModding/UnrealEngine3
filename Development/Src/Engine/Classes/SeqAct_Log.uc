class SeqAct_Log extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

/** Should this message be drawn on the screen as well as placed in the log? */
var() bool bOutputToScreen;

defaultproperties
{
	ObjName="Log"
	bOutputToScreen=true
	VariableLinks(0)=(ExpectedType=class'SeqVar_String',LinkDesc="String",MinVars=0)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Float",MinVars=0)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Bool",MinVars=0)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Object',LinkDesc="Object",MinVars=0)
	VariableLinks(4)=(ExpectedType=class'SeqVar_Int',LinkDesc="Int",MinVars=0)
}