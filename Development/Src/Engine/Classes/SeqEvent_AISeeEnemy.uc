class SeqEvent_AISeeEnemy extends SequenceEvent
	native(Sequence);

cpptext
{
	UBOOL CheckActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest = 0)
	{
		if (inOriginator != NULL &&
			inInstigator != NULL &&
			(MaxSightDistance <= 0.f ||
			 (inOriginator->Location-inInstigator->Location).Size() <= MaxSightDistance))
		{
			return Super::CheckActivate(inOriginator,inInstigator,bTest);
		}
		else
		{
			return 0;
		}
	}
};

/** Max distance before allowing activation */
var() float MaxSightDistance;

defaultproperties
{
	ObjName="AI: See Enemy"
	MaxSightDistance=256.f
}
