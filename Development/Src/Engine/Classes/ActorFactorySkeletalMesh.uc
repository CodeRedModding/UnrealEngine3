class ActorFactorySkeletalMesh extends ActorFactory
	config(Editor)
	native;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
	virtual UBOOL CanCreateActor();
	virtual void AutoFillFields();
	virtual FString GetMenuName();
}

var()	SkeletalMesh	SkeletalMesh;
var()	AnimSet			AnimSet;
var()	name			AnimSequenceName;

defaultproperties
{
	MenuName="Add SkeletalMesh"
	NewActorClass=class'Engine.SkeletalMeshActor'
}