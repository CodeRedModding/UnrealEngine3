/*=============================================================================
	ActorFactoryApexClothing.uc: Implement APEX Clothing Actor Factory
	Copyright 2008-2009 NVIDIA corporation..
=============================================================================*/
class ActorFactoryApexClothing extends ActorFactorySkeletalMesh
	config(Editor)
	hidecategories(Object)
	native(Physics);
	
/** List of clothing assets associated with each material int this mesh. */	
var()	array<ApexClothingAsset>    ClothingAssets;

/** Allows setting the RBChannel flag on the spawned rigid body's StaticMeshComponent. */
var() const ERBCollisionChannel		ClothingRBChannel;

/** Define the channels with which this actor will collide. */
var() const RBCollisionChannelContainer ClothingRBCollideWithChannels;

cpptext
{
	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );
	virtual UBOOL CanCreateActor(FString& OutErrorMsg, UBOOL bFromAssetOnly = FALSE);
	virtual void AutoFillFields(class USelection* Selection);
	virtual FString GetMenuName();
}

defaultproperties
{
	MenuName="Add Clothing";
	NewActorClass=class'Engine.SkeletalMeshActor'
	GameplayActorClass=class'Engine.SkeletalMeshActorSpawnable'
	
 	ClothingRBChannel=RBCC_Clothing
	ClothingRBCollideWithChannels=(Default=TRUE,BlockingVolume=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE,ClothingCollision=TRUE)
}
