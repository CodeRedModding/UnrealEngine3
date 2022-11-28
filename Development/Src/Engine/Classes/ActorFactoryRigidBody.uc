class ActorFactoryRigidBody extends ActorFactoryStaticMesh
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

var()	bool	bStartAwake;
var()	bool	bDamageAppliesImpulse;
var()	vector	InitialVelocity;

cpptext
{
	virtual UBOOL CanCreateActor();
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
}

defaultproperties
{
	MenuName="Add RigidBody"
	MenuPriority=0
	NewActorClass=class'Engine.KActor'

	bStartAwake=true
	bDamageAppliesImpulse=true
}