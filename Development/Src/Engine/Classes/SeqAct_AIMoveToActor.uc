class SeqAct_AIMoveToActor extends SeqAct_Latent
	native(Sequence);

cpptext
{
	virtual UBOOL UpdateOp(FLOAT deltaTime);
}

/** Should this move be interruptable? */
var() bool bInterruptable;

defaultproperties
{
	ObjName="AI: Move To Actor"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Destination")
}
