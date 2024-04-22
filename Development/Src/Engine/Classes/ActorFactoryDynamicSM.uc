/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryDynamicSM extends ActorFactory
	config(Editor)
	native
	abstract;

cpptext
{
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
	virtual void PostLoad();
}

var()	StaticMesh		StaticMesh;
var()	vector			DrawScale3D;

/**
 *	For encroachers, don't do the overlap check when they move. You will not get touch events for this actor moving, but it is much faster.
 *	So if you want touch events from volumes or triggers you need to set this to be FALSE.
 *	This is an optimisation for large numbers of PHYS_RigidBody actors for example.
 *	@see Actor.uc bNoEncroachCheck
 */
var()	bool			bNoEncroachCheck;
var()	bool			bNotifyRigidBodyCollision;
var()	ECollisionType		CollisionType;
var()   bool            bBlockRigidBody;

/** Try and use physics hardware for this spawned object. */
var()	bool			bUseCompartment;

/** If false, primitive does not cast dynamic shadows. */
var()	bool			bCastDynamicShadow;

defaultproperties
{
	DrawScale3D=(X=1,Y=1,Z=1)
	CollisionType=COLLIDE_NoCollision
	bCastDynamicShadow=true
	bBlockRigidBody=FALSE
}
