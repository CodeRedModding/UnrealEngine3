/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_DelaySwitch extends SeqAct_Latent
	deprecated
	native(Sequence);

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
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
		Super::PostEditChangeProperty(PropertyChangedEvent);
	}

	void Activated()
	{
		// reset the link index
		CurrentIdx = 0;
		// figure out the total delay
		TArray<FLOAT*> floatVars;
		GetFloatVars(floatVars,TEXT("Delay"));
		SwitchDelay = 0.f;
		for (INT idx = 0; idx < floatVars.Num(); idx++)
		{
			SwitchDelay += *(floatVars(idx));
		}
		NextLinkTime = SwitchDelay;
	}

	UBOOL UpdateOp(FLOAT deltaTime)
	{
		if (NextLinkTime <= 0.f)
		{
			if( CurrentIdx < OutputLinks.Num() && !OutputLinks(CurrentIdx).bDisabled &&
				!(OutputLinks(CurrentIdx).bDisabledPIE && GIsEditor))
			{
				// activate the new link
				OutputLinks(CurrentIdx).bHasImpulse = 1;
				// fill any variables attached
				TArray<INT*> intVars;
				GetIntVars(intVars,TEXT("Active Link"));
				for (INT idx = 0; idx < intVars.Num(); idx++)
				{
					// offset by 1 for non-programmer friendliness
					*(intVars(idx)) = CurrentIdx + 1;
				}
				// and increment the link index
				CurrentIdx++;
			}
			NextLinkTime = SwitchDelay;
		}
		else
		{
			NextLinkTime -= deltaTime;
		}
		return (CurrentIdx >= OutputLinks.Num());
	}

	void DeActivated()
	{
	}
};

var() int							LinkCount;

var transient int					CurrentIdx;
var transient float					SwitchDelay;
var transient float					NextLinkTime;

defaultproperties
{
	ObjName="Delayed"
	ObjCategory="Switch"
	LinkCount=1
	OutputLinks(0)=(LinkDesc="Link 1")

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Delay")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Int',LinkDesc="Active Link",MinVars=0)
}
