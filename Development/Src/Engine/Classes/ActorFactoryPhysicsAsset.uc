/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryPhysicsAsset extends ActorFactory
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

var()	PhysicsAsset		PhysicsAsset;
var()	SkeletalMesh		SkeletalMesh;

var()	bool				bStartAwake;
var()	bool				bDamageAppliesImpulse;
var()	bool				bNotifyRigidBodyCollision;
var()	vector				InitialVelocity;
var()	vector				DrawScale3D;

/** Try and use physics hardware for this spawned object. */
var()	bool	bUseCompartment;

/** If false, primitive does not cast dynamic shadows. */
var()	bool	bCastDynamicShadow;

cpptext
{
	virtual void PreSave();

	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );

	/**
	 * If the ActorFactory thinks it could create an Actor with the current settings.
	 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
	 *
	 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
	 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
	 * @return	True if the actor can be created with this factory
	 */
	virtual UBOOL CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly = FALSE );

	virtual void AutoFillFields(class USelection* Selection);
	virtual FString GetMenuName();
}

defaultproperties
{
	MenuName="Add PhysicsAsset"
	NewActorClass=class'Engine.KAsset'
	GameplayActorClass=class'Engine.KAssetSpawnable'

	DrawScale3D=(X=1,Y=1,Z=1)
	bStartAwake=true
	bDamageAppliesImpulse=true
	bCastDynamicShadow=true
}
