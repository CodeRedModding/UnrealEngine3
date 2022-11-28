class ActorFactoryRoute extends ActorFactory
	config(Editor)
	native;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
	virtual UBOOL CanCreateActor();
};

defaultproperties
{
	MenuName="Add Route"
	NewActorClass=class'Engine.Route'
	bPlaceable=true
}
