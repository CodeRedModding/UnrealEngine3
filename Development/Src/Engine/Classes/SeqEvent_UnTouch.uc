class SeqEvent_UnTouch extends SequenceEvent
	native(Sequence);

cpptext
{
	UBOOL CheckActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest = 0);
};

var() array<class<Actor> >		ClassProximityTypes;

defaultproperties
{
	ObjName="UnTouch"
   	ClassProximityTypes(0)=class'Pawn'
}