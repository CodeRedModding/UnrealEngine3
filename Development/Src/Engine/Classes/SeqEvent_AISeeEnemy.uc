/**
 * Event which is triggered by the AI code when an NPC sees an enemy pawn.
 * Originator: the pawn associated with the NPC
 * Insigator: the enemy PC that has been spotted.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_AISeeEnemy extends SequenceEvent
	native(Sequence);

cpptext
{
	virtual UBOOL CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest=FALSE, TArray<INT>* ActivateIndices = NULL, UBOOL bPushTop = FALSE)
	{
		if (InOriginator != NULL &&
			InInstigator != NULL &&
			(MaxSightDistance <= 0.f ||
			 (InOriginator->Location-InInstigator->Location).Size() <= MaxSightDistance))
		{
			return Super::CheckActivate(InOriginator,InInstigator,bTest,ActivateIndices, bPushTop);
		}
		else
		{
			return FALSE;
		}
	}
};

/** Max distance before allowing activation */
var() float MaxSightDistance;

defaultproperties
{
	ObjName="See Enemy"
	ObjCategory="AI"
	MaxSightDistance=0.f
}
