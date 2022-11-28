class ActorFactoryPhysicsAsset extends ActorFactory
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

var()	PhysicsAsset		PhysicsAsset;
var()	SkeletalMesh		SkeletalMesh;

var()	bool				bStartAwake;
var()	bool				bDamageAppliesImpulse;
var()	vector				InitialVelocity;
var()	vector				DrawScale3D;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
	virtual UBOOL CanCreateActor();
	virtual void AutoFillFields();
	virtual FString GetMenuName();
}

defaultproperties
{
	MenuName="Add PhysicsAsset"
	NewActorClass=class'Engine.KAsset'

	DrawScale3D=(X=1,Y=1,Z=1)
	bStartAwake=true
	bDamageAppliesImpulse=true
}