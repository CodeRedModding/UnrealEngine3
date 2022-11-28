class ActorFactoryStaticMesh extends ActorFactory
	config(Editor)
	native;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
	virtual UBOOL CanCreateActor();
	virtual void AutoFillFields();
	virtual FString GetMenuName();
}

var()	StaticMesh		StaticMesh;
var()	vector			DrawScale3D;

defaultproperties
{
	DrawScale3D=(X=1,Y=1,Z=1)

	MenuName="Add StaticMesh"
	MenuPriority=2
	NewActorClass=class'Engine.StaticMeshActor'
}