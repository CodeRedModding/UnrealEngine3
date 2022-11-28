class SeqEvent_Touch extends SequenceEvent
	native(Sequence);

cpptext
{
	UBOOL CheckActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest = 0);
};

//==========================
// Base variables

var() array<class<Actor> >		ClassProximityTypes;

defaultproperties
{
	ObjName="Touch"
	ClassProximityTypes(0)=class'Pawn'
}