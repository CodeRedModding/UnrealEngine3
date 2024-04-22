/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryRigidBody extends ActorFactoryDynamicSM
	config(Editor)
	collapsecategories
	hidecategories(Object)
	native;

/** Should spawned Actor start simulating as soon as its created, or be 'asleep' until hit. */
var()	bool	bStartAwake;

/** Sets the bDamageAppliesImpulse flag on the new Actor. */
var()	bool	bDamageAppliesImpulse;

/** Indicates if the initial velocity settings below should be considered in the world space or local space of the spawn target actor. */
var()	bool	bLocalSpaceInitialVelocity;

/** Velocity that new rigid bodies will have when created. In the ref frame of the spawn target actor.  */
var()	vector	InitialVelocity;

/**
 *	If valid, Velocity added to InitialVelocity when creating actor.
 *	This is here in addition to InitialVelocity to maintain backwards compatibility.
 */
var()	DistributionVector	AdditionalVelocity;

/**
 *	If valid, Angular Velocity given to newly spawned Actor.
 */
var()	DistributionVector	InitialAngularVelocity;

/** Allows setting the RBChannel flag on the spawned rigid body's StaticMeshComponent. */
var()	ERBCollisionChannel		RBChannel;


/** Enable 'Stay upright' torque, that tries to keep Z axis of KActor pointing along world Z */
var()	bool		bEnableStayUprightSpring;

/** Torque applied to try and keep KActor horizontal. */
var()	float		StayUprightTorqueFactor;

/** Max torque that can be applied to try and keep KActor horizontal */
var()	float		StayUprightMaxTorque;

cpptext
{
	// UObject interface
	virtual void PostLoad();

	/**
	 * If the ActorFactory thinks it could create an Actor with the current settings.
	 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
	 *
	 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
	 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
	 * @return	True if the actor can be created with this factory
	 */
	virtual UBOOL CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly = FALSE );

	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );
}

defaultproperties
{
	MenuName="Add RigidBody"

	NewActorClass=class'Engine.KActor'
	GameplayActorClass=class'Engine.KActorSpawnable'

	bNoEncroachCheck=true
	bStartAwake=true
	bDamageAppliesImpulse=true
	CollisionType=COLLIDE_BlockAll
	RBChannel=RBCC_GameplayPhysics
	bBlockRigidBody=TRUE

	StayUprightTorqueFactor=1000.0
	StayUprightMaxTorque=1500.0
}
