/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * @todo - this should be a conditional
 */
class SeqAct_RangeSwitch extends SequenceAction
	deprecated
	native(Sequence);

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (OutputLinks.Num() < Ranges.Num())
		{
			// keep adding, updating the description
			while (OutputLinks.Num() < Ranges.Num())
			{
				OutputLinks.AddZeroed();
			}
		}
		else
		if (OutputLinks.Num() > Ranges.Num())
		{
			while (OutputLinks.Num() > Ranges.Num())
			{
				//FIXME: any cleanup needed for each link, or can we just mass delete?
				OutputLinks.Remove(OutputLinks.Num()-1);
			}
		}
		// match all the link descriptions to the range values
		for (INT idx = 0; idx < Ranges.Num(); idx++)
		{
			OutputLinks(idx).LinkDesc = FString::Printf(TEXT("%d - %d"),Ranges(idx).Min,Ranges(idx).Max);
		}
		Super::PostEditChangeProperty(PropertyChangedEvent);
	}

	virtual void Activated()
	{
		USequenceOp::Activated();

		// get all of the attached int vars
		TArray<INT*> intVars;
		GetIntVars(intVars,TEXT("Index"));
		// and activate the matching outputs
		for (INT idx = 0; idx < intVars.Num(); idx++)
		{
			INT activeIdx = *(intVars(idx));
			for (INT rangeIdx = 0; rangeIdx < Ranges.Num(); rangeIdx++)
			{
				if (activeIdx >= Ranges(rangeIdx).Min	&&
					activeIdx <= Ranges(rangeIdx).Max	&&
					!OutputLinks(rangeIdx).bDisabled	&&
					!(OutputLinks(rangeIdx).bDisabledPIE && GIsEditor)
					)
				{
					OutputLinks(rangeIdx).bHasImpulse = 1;
				}
			}
		}
	}

	void DeActivated()
	{
		// do nothing, already activated output links
	}
};

struct native SwitchRange
{
	var() int Min;
	var() int Max;
};

var() editinline array<SwitchRange> Ranges;

defaultproperties
{
	ObjName="Ranged"
	ObjCategory="Switch"
	OutputLinks.Empty
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="Index")
}
