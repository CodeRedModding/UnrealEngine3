/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqCond_ShowGore extends SequenceCondition
	native(Sequence);

cpptext
{
	virtual void Activated()
	{
		USequenceOp::Activated();

		if( GWorld && GWorld->GetWorldInfo() && GWorld->GetWorldInfo()->GRI && 
			GWorld->GetWorldInfo()->GRI->eventShouldShowGore() )
		{
			OutputLinks(0).bHasImpulse = TRUE;
		}
		else
		{
			OutputLinks(1).bHasImpulse = TRUE;
		}
	}
}

defaultproperties
{
	ObjName="Show Gore"

	OutputLinks(0)=(LinkDesc="Allowed")
	OutputLinks(1)=(LinkDesc="Disallowed")
}
