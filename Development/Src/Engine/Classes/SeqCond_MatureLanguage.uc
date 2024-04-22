/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqCond_MatureLanguage extends SequenceCondition
	native(Sequence);

cpptext
{
	virtual void Activated()
	{
		USequenceOp::Activated();

		if( GEngine && GEngine->bAllowMatureLanguage )
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
	ObjName="Mature Language Allowed"

	OutputLinks(0)=(LinkDesc="Allowed")
	OutputLinks(1)=(LinkDesc="Disallowed")
}
