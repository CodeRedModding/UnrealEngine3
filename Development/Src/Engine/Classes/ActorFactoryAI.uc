class ActorFactoryAI extends ActorFactory
	config(Editor)
	native;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
};

var() class<AIController>			ControllerClass;
var() class<Pawn>					PawnClass;

var() array<class<Inventory> >		InventoryList;
var() int							TeamIndex;

defaultproperties
{
	ControllerClass=class'AIController'
	PawnClass=class'Pawn'

	TeamIndex=255
	bPlaceable=false
}
