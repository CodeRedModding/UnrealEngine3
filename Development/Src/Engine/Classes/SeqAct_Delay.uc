class SeqAct_Delay extends SeqAct_Latent
	native(Sequence);

cpptext
{
	void Activated();
	UBOOL UpdateOp(FLOAT deltaTime);
};

/** Default duration to use if no variables are linked */
var() float DefaultDuration;

/** Remaining time until this action completes */
var transient float RemainingTime;

defaultproperties
{
	ObjName="Delay"

	DefaultDuration=1.f
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Duration")
}
