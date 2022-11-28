class SeqAct_CauseDamage extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
}

var() class<DamageType>		DamageType;

var transient float			DamageAmount;

defaultproperties
{
	ObjName="Cause Damage"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Amount")
}
