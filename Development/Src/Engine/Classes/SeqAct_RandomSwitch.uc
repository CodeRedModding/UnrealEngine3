class SeqAct_RandomSwitch extends SeqAct_Switch
	native(Sequence);

cpptext
{
	virtual void Activated()
	{
		// pick a random link to activate
		INT outIdx = appRand() % OutputLinks.Num();
		OutputLinks(outIdx).bHasImpulse = 1;
		// fill any variables attached
		TArray<INT*> intVars;
		GetIntVars(intVars,TEXT("Active Link"));
		for (INT idx = 0; idx < intVars.Num(); idx++)
		{
			// offset by 1 for non-programmer friendliness
			*(intVars(idx)) = outIdx + 1;
		}
	}
};

defaultproperties
{
	ObjName="Switch - Random"
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="Active Link",bWriteable=true,MinVars=0)
}
