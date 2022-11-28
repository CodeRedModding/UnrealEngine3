class SeqAct_GetCash extends SequenceAction
	native;

cpptext
{
	virtual void Activated()
	{
		TArray<UObject**> objVars;
		GetObjectVars(objVars,TEXT("Target"));
		INT cashAmt = 0;
		for (INT idx = 0; idx < objVars.Num(); idx++)
		{
			AWarPC *pc = Cast<AWarPC>(*(objVars(idx)));
			if (pc != NULL)
			{
				cashAmt += pc->Cash;
			}
		}
		TArray<INT*> intVars;
		GetIntVars(intVars,TEXT("Cash"));
		for (INT idx = 0; idx < intVars.Num(); idx++)
		{
			*(intVars(idx)) = cashAmt;
		}
	}
}

defaultproperties
{
	ObjName="Get Cash"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Int',LinkDesc="Cash",bWriteable=true)
}
