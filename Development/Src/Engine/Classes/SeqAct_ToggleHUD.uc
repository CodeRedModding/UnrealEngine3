class SeqAct_ToggleHUD extends SequenceAction;

defaultproperties
{
	ObjName="Toggle HUD"

	InputLinks(0)=(LinkDesc="Show")
	InputLinks(1)=(LinkDesc="Hide")
	InputLinks(2)=(LinkDesc="Toggle")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Player',LinkDesc="Target")
}
