//=============================================================================
// SVehicle
//=============================================================================

class SVehicle extends Vehicle
	native(Physics)
	abstract;

cpptext
{
	// Actor interface.
	virtual void physRigidBody(FLOAT DeltaTime);
	virtual void TickSimulated( FLOAT DeltaSeconds );
	virtual void TickAuthoritative( FLOAT DeltaSeconds );
	virtual void setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV);
	virtual void PostNetReceiveLocation();

#ifdef WITH_NOVODEX
	virtual void ModifyNxActorDesc(NxActorDesc& ActorDesc);
	virtual void PostInitRigidBody(NxActor* nActor);
#endif

	// SVehicle interface.

	/** 
	 *	This is called by ModifyNxActorDesc before it creates wheel capsule shapes and materials etc.
	 *	Allows subclasses to do last-minute paramter setting on the wheel. Don't create/destroy anything here though!
	 */
	virtual void PreInitVehicle() {}

	/**
	 *	SVehicle will update SlipVel and SlipAngle, then this allows a subclass to update the properties such as
	 *	Steer, DriveForce etc. Afterwards they will be applied to the vehicles physics.
	 */
	virtual void UpdateVehicle(FLOAT DeltaTime) {}
}

/** Data for each wheel. */
var (SVehicle) editinline export	array<SVehicleWheel>		Wheels; 

/** OUTPUT: True if _any_ SVehicleWheel is currently touching the ground (ignores contacts with chassis etc) */
var	const bool			bVehicleOnGround; 

defaultproperties
{
	Begin Object Class=SkeletalMeshComponent Name=SVehicleMesh
		BlockZeroExtent=true
		BlockRigidBody=true
		CollideActors=true
		RootBoneOption[0]=RBA_Discard
		RootBoneOption[1]=RBA_Discard
		RootBoneOption[2]=RBA_Discard
		bDiscardRootRotation=1
	End Object
	Mesh=SVehicleMesh
	CollisionComponent=SVehicleMesh

    Physics=PHYS_RigidBody
	bEdShouldSnap=true
	bStatic=false
	bCollideActors=true
	bCollideWorld=false
    bProjTarget=true
	bBlockActors=true
	bWorldGeometry=false
	bCanBeBaseForPawns=true
	bAlwaysRelevant=false
	RemoteRole=ROLE_SimulatedProxy
	bNetInitialRotation=true
}
