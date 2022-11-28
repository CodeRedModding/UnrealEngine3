class SeqAct_Switch extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void PostEditChange(UProperty* PropertyThatChanged)
	{
		Super::PostEditChange(PropertyThatChanged);
		// force at least one output link
		if (LinkCount <= 0)
		{
			LinkCount = 1;
		}
		if (OutputLinks.Num() < LinkCount)
		{
			// keep adding, updating the description
			while (OutputLinks.Num() < LinkCount)
			{
				INT idx = OutputLinks.AddZeroed();
				OutputLinks(idx).LinkDesc = FString::Printf(TEXT("Link %d"),idx+1);
			}
		}
		else
		if (OutputLinks.Num() > LinkCount)
		{
			while (OutputLinks.Num() > LinkCount)
			{
				//FIXME: any cleanup needed for each link, or can we just mass delete?
				OutputLinks.Remove(OutputLinks.Num()-1);
			}
		}
	}

	virtual void Activated()
	{
		// get all of the attached int vars
		TArray<INT*> intVars;
		GetIntVars(intVars,TEXT("Index"));
		// and activate the matching output
		for (INT idx = 0; idx < intVars.Num(); idx++)
		{
			INT activeIdx = *(intVars(idx)) - 1;
			if (activeIdx >= 0 &&
				activeIdx < OutputLinks.Num())
			{
				OutputLinks(activeIdx).bHasImpulse = 1;
			}
			// increment the int vars
			*(intVars(idx)) += IncrementAmount;
		}
	}

	void DeActivated()
	{
		// do nothing, already activated output links
	}
};

/** Total number of links to expose */
var() int LinkCount;

/** Number to increment attached variables upon activation */
var() int IncrementAmount;

defaultproperties
{
	ObjName="Switch"

	LinkCount=1
	IncrementAmount=1
	OutputLinks(0)=(LinkDesc="Link 1")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="Index")
}
