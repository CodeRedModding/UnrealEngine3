class SeqAct_AIShootAtTarget extends SeqAct_Latent
	native;

cpptext
{
	virtual UBOOL UpdateOp(FLOAT deltaTime);
}

/** Should this action be interruptable? */
var() bool bInterruptable;

defaultproperties
{
	ObjName="AI: Shoot At Target"

	InputLinks(0)=(LinkDesc="Start")
	InputLinks(1)=(LinkDesc="Stop")

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Shoot Target")
}
