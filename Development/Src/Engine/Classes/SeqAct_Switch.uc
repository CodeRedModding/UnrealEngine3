/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Switch extends SequenceAction
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

	virtual void Activated()
	{
		USequenceOp::Activated();

		// activate each of the indices
		for (INT Idx = 0; Idx < Indices.Num(); Idx++)
		{
			INT ActiveIdx = Indices(Idx) - 1;
			if (ActiveIdx >= 0 &&
				ActiveIdx < OutputLinks.Num())
			{
				if (!OutputLinks(ActiveIdx).bDisabled && 
					!(OutputLinks(ActiveIdx).bDisabledPIE && GIsEditor))
				{
					OutputLinks(ActiveIdx).bHasImpulse = TRUE;
					if (bAutoDisableLinks)
					{
						OutputLinks(ActiveIdx).bDisabled = TRUE;
					}
				}
			}
			// increment the indices
			if (IncrementAmount != 0)
			{
				if (bLooping)
				{
					Indices(Idx) = 1 + ((Indices(Idx) - 1 + IncrementAmount) % OutputLinks.Num());
				}
				else
				{
					Indices(Idx) += IncrementAmount;
				}
			}
		}
	}

	virtual void UpdateObject()
	{
		// save the output links
		TArray<FSeqOpOutputLink> SavedOutputLinks = OutputLinks;
		Super::UpdateObject();
		OutputLinks.Empty();
		OutputLinks = SavedOutputLinks;
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

/** Loop index back to beginning to cycle */
var() bool bLooping;

/** List of links to activate */
var() array<int> Indices;

/** Automatically disable an output once its activated? */
var() bool bAutoDisableLinks;


defaultproperties
{
	ObjName="Switch"
	ObjCategory="Switch"

	Indices(0)=1
	LinkCount=1
	IncrementAmount=1
	OutputLinks(0)=(LinkDesc="Link 1")
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="Index",PropertyName=Indices)
}
