class ActorFactoryEmitter extends ActorFactory
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
	virtual UBOOL CanCreateActor();
	virtual void AutoFillFields();
	virtual FString GetMenuName();
}

var()	ParticleSystem		ParticleSystem;

defaultproperties
{
	MenuName="Add Emitter"
	NewActorClass=class'Engine.Emitter'
}