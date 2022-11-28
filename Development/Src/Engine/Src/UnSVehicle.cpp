/*=============================================================================
	UnSVehicle.cpp: Skeletal vehicle
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"

#ifdef WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(ASVehicle);
IMPLEMENT_CLASS(USVehicleWheel);

/**
 *	Calculate the orienation matrix of wheel capsule shape. We want the capsule axis (its 'Y') to point down in Actor space.
 *	X and Z are rotated by whatever the steering angle is.
 *
 *	@param ActorToWorld Actor-to-world transformation.
 *	@param LocalToWorld Component-to-world transformation.
 *  @param SteerAngle	Desired angle of steering, in radians.
 */
static FMatrix MakeWheelRelRotMatrix(const FMatrix& ActorToWorld, const FMatrix& LocalToWorld, FLOAT SteerAngle)
{
	// Capsule directions in ACTOR space;
	FVector ActorCapX( appCos(SteerAngle),	appSin(SteerAngle),	0	);
	FVector ActorCapY( 0,					0,					-1	);
	FVector ActorCapZ( -appSin(SteerAngle),	appCos(SteerAngle),	0	);

	// Capsule directions in WORLD space.
	FVector WorldCapX = ActorToWorld.TransformNormal(ActorCapX);
	FVector WorldCapY = ActorToWorld.TransformNormal(ActorCapY);
	FVector WorldCapZ = ActorToWorld.TransformNormal(ActorCapZ);

	// Capsule directions in COMPONENT space.
	// Because root bone (chassis bone) must be at origin of SkeletalMeshComponent, this is also the transform relative to parent bone.
	FVector ComponentCapX = LocalToWorld.InverseTransformNormal(WorldCapX);
	FVector ComponentCapY = LocalToWorld.InverseTransformNormal(WorldCapY);
	FVector ComponentCapZ = LocalToWorld.InverseTransformNormal(WorldCapZ);
	
	return FMatrix(ComponentCapX, ComponentCapY, ComponentCapZ, FVector(0.f));
}


#ifdef WITH_NOVODEX

/** 
 *	This is called from URB_BodyInstance::InitBody, and lets us add in the capsules for each wheel before creating the vehicle rigid body. 
 *	The rigid body is created at the skelmesh transform, so the wheel geometry relative transforms are just the 
 *	bone transforms (assuming wheels are direct children of chassis).
 */
void ASVehicle::ModifyNxActorDesc(NxActorDesc& ActorDesc)
{
	check(Mesh);
	check(Mesh == CollisionComponent);
	check(Mesh->SkeletalMesh);

	// Warn if trying to do something silly like non-uniform scale a vehicle.
	FVector TotalScale = DrawScale * DrawScale3D;
	if( !TotalScale.IsUniform() )
	{
		debugf( TEXT("ASVehicle::ModifyNxActorDesc : Can only uniformly scale SVehicles. (%s)"), GetName() );
		return;
	}

	// Let subclass do any last minute run-time 
	PreInitVehicle();

	USkeletalMesh* SkelMesh = Mesh->SkeletalMesh;


	// SVehicles should always discard root transform so root bone and SkeletalMeshComponent are at same location.
	check(Mesh->RootBoneOption[0] == RBA_Discard);
	check(Mesh->RootBoneOption[1] == RBA_Discard);
	check(Mesh->RootBoneOption[2] == RBA_Discard);

	FMatrix LocalWheelRot = MakeWheelRelRotMatrix( LocalToWorld(), Mesh->LocalToWorld, 0.f );
	check( LocalWheelRot.Determinant() >= 0.f );

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		check(vw);

		// Get location of this wheel bone in component space.
		INT BoneIndex = SkelMesh->MatchRefBone(vw->BoneName);
		if(BoneIndex == INDEX_NONE)
		{
			debugf( TEXT("ASVehicle::ModifyNxActorDesc : Bone (%s) not found in Vehicle (%s) SkeletalMesh (%s)"), *vw->BoneName, GetName(), SkelMesh->GetName() );
			check(BoneIndex != INDEX_NONE);
		}
		
		// Get wheel bone transform relative to parent. Parent should be root, which should be at same location as SkeletalMeshComponent.
		FMatrix WheelBoneMatrix = FMatrix( SkelMesh->RefSkeleton(BoneIndex).BonePos.Position, SkelMesh->RefSkeleton(BoneIndex).BonePos.Orientation );

		// Wheel position in component space.
		vw->WheelPosition = WheelBoneMatrix.TransformFVector(vw->BoneOffset);
		
		FMatrix CapsuleTM = LocalWheelRot;
		CapsuleTM.SetOrigin( vw->WheelPosition );

		// Create NxCapsuleShapeDesc and add to  ActorDesc.shapes. UserData on each one will be the USVehicleWheel.
		// JTODO URGENT: Clear up CapsuleDesc after NxActor is created. Maybe in PostInitRigidBody?
		NxCapsuleShapeDesc* CapsuleDesc = new NxCapsuleShapeDesc;

		NxMat34 nCapsuleTM = U2NTransform(CapsuleTM);
		CapsuleDesc->localPose = nCapsuleTM;

		CapsuleDesc->height = 2 * vw->WheelRadius * U2PScale;
		CapsuleDesc->radius = 0.1f; // Does this make any difference?
		CapsuleDesc->flags = NX_SWEPT_SHAPE;
		CapsuleDesc->userData = vw;

		// Create a material for this wheel.
		// JTODO URGENT: Clear up the material when vehicle destroyed!
		NxSpringDesc* SpringDesc = new NxSpringDesc;
		SpringDesc->spring = vw->SuspensionStiffness;
		SpringDesc->damper = vw->SuspensionDamping;
		SpringDesc->targetValue = vw->SuspensionBias;

		NxMaterial WheelMaterial;
		WheelMaterial.restitution = vw->Restitution;
		WheelMaterial.staticFriction = vw->LatFrictionStatic;
		WheelMaterial.dynamicFriction = vw->LatFrictionDynamic;
		WheelMaterial.staticFrictionV = 0.f;
		WheelMaterial.dynamicFrictionV = 0.0f;
		WheelMaterial.dirOfAnisotropy.set(1.f, 0.f, 0.f);
		WheelMaterial.frictionCombineMode = NX_CM_MULTIPLY;
		WheelMaterial.flags	|= (NX_MF_SPRING_CONTACT | NX_MF_ANISOTROPIC);
		WheelMaterial.programData	= SpringDesc;

		vw->WheelMaterialIndex = GNovodexSDK->addMaterial(WheelMaterial);

		CapsuleDesc->materialIndex = vw->WheelMaterialIndex;

		ActorDesc.shapes.push_back( CapsuleDesc );

		// We set the Actors Group to '1' to indicate its a wheel. 
		// We will get a callback for all wheel/ground contacts and can retrieve contact position from there.
		ActorDesc.group = 1;
	}	
}

void ASVehicle::PostInitRigidBody(NxActor* nActor)
{
	// From the NxActor, iterate over shapes to find ones that point to USVehicleWheels, and store a pointer to the NxShape.
	// We'll need that later for changing the direction for steerin.

	check(Mesh);
	check(Mesh == CollisionComponent);
	check(Mesh->SkeletalMesh);

	INT NumShapes = nActor->getNbShapes();
	NxShape** Shapes = nActor->getShapes();

	for(INT i=0; i<NumShapes; i++)
	{
		// This assumes that the only NxShapes with UserData are vehicle wheels.. should probably be more generic...
		NxShape* nShape = Shapes[i];
		if(nShape->userData)
		{
			USVehicleWheel* vw = (USVehicleWheel*)(nShape->userData);
			vw->RayShape = nShape;
		}
	}

	// Check we got a shape for every wheel.
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		check(vw);
		check(vw->RayShape);
	}
}
#endif // WITH_NOVODEX


/** This is to avoid doing APawn::PostNetReceiveLocation fixup stuff. */
void ASVehicle::PostNetReceiveLocation()
{
	AActor::PostNetReceiveLocation();
}


/** An extra check to make sure physics isn't anything other than PHYS_RigidBody or PHYS_None. */
void ASVehicle::setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV)
{
	check(Physics == PHYS_RigidBody || Physics == PHYS_None);

	Super::setPhysics(NewPhysics, NewFloor, NewFloorV);
}

/** So that full ticking is done on client. */
void ASVehicle::TickAuthoritative( FLOAT DeltaSeconds )
{
	check(Physics == PHYS_RigidBody || Physics == PHYS_None); // karma vehicles should always be in PHYS_Karma

	eventTick(DeltaSeconds);
	ProcessState( DeltaSeconds );
	UpdateTimers(DeltaSeconds );

	// Update LifeSpan.
	if( LifeSpan!=0.0f )
	{
		LifeSpan -= DeltaSeconds;
		if( LifeSpan <= 0.0001f )
		{
			GetLevel()->DestroyActor( this );
			return;
		}
	}

	// Perform physics.
	if ( !bDeleteMe && Physics != PHYS_None )
	{
		performPhysics( DeltaSeconds );
	}

	if( CollisionComponent && CollisionComponent->RigidBodyIsAwake() )
	{
		NetUpdateTime = Level->TimeSeconds - 1; // force quick net update
	}
}

/** So that full ticking is done on client. */
void ASVehicle::TickSimulated( FLOAT DeltaSeconds )
{
	TickAuthoritative(DeltaSeconds);
}

void ASVehicle::physRigidBody(FLOAT DeltaTime)
{
	check(Mesh);
	check(Mesh == CollisionComponent);
	check(Mesh->SkeletalMesh);

	// Do usual rigid body stuff. Will move position of Actor.
	Super::physRigidBody(DeltaTime);

#ifdef WITH_NOVODEX
	// Get Novodex rigid body.
	NxActor* nActor = GetNxActor();
	if(!nActor)
	{
		return;
	}

	/////////////// UPDATE OUTPUT INFORMATION IN SVEHICLEWHEELS /////////////// 	

	bVehicleOnGround = false;

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		// SuspensionPosition, TireLoad and bWheelOnGround will have been filled in (if wheel is on ground) by physics callback.


		FMatrix LocalWheelRot = MakeWheelRelRotMatrix( LocalToWorld(), Mesh->LocalToWorld, vw->Steer );

		// Find wheel center in world space.
		FVector WheelCenter = Mesh->LocalToWorld.TransformFVector( vw->WheelPosition );
		NxVec3 nWheelCenter = U2NPosition(WheelCenter);

		// Find wheel roll direction vector. This is the 'X' axis of the capsule transform.
		FVector WheelDir = Mesh->LocalToWorld.TransformNormal( LocalWheelRot.GetAxis(0) );

		// Find suspension 'up' vector.
		FVector WheelNormal = Mesh->LocalToWorld.TransformNormal( LocalWheelRot.GetAxis(1) );

		// Find wheel axle direction vector. This is the 'Z' axis of the capsule transform.
		FVector WheelAxle = Mesh->LocalToWorld.TransformNormal( LocalWheelRot.GetAxis(2) ); 

		// Find linear velocity of wheel in world space.
		NxVec3 nWheelLinVel = nActor->getPointVelocity(nWheelCenter);
		FVector WheelLinVel = N2UPosition(nWheelLinVel);

		// Project along normal and subtract to find wheel velocity in tyre-contact plane.
		FLOAT NormVelMag = WheelLinVel | WheelNormal;		
		FVector PlaneVel = WheelLinVel - (NormVelMag * WheelNormal);

		// Find magnitude in wheel's 'forward' direction.
		FLOAT GroundLongVel = PlaneVel | WheelDir;

		// Then find velocity of wheel surface
		FLOAT WheelLongVel = vw->WheelRadius * vw->SpinVel;

		// Store the relative slip velocity for use in UpdateVehicle.
		vw->SlipVel = WheelLongVel - GroundLongVel;

		// Find angle between wheel direction and sliding direction (slip angle)
		// If wheel is not moving, assume slip angle of zero
		if( PlaneVel.SizeSquared() < 0.05f * 0.05f )
		{
			vw->SlipAngle = 0.0f;
		}
		else
		{
			// Get direction of velocity in tyre-contact plane
			FVector PlaneVelDir = PlaneVel.SafeNormal();

			FLOAT DirDot =  PlaneVelDir | WheelDir;
			DirDot = Clamp<FLOAT>(DirDot, 0.0f, 1.0f); // Only create slip angles up to 90 degrees

			vw->SlipAngle = ( 180.0f/PI ) * appAcos( DirDot ); // Note this doesn't give a sign
		}
	}

	/////////////// DO ENGINE MODEL & UPDATE WHEEL FORCES /////////////// 	

	UpdateVehicle(DeltaTime);

	/////////////// APPLY WHEEL FORCES AND UPDATE GRAPHICS /////////////// 	


	UBOOL bWheelsMoving = false;

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		// Orient wheel capsule shape to correspond with steering.
		check(vw->RayShape);
		NxShape* RayShape = (NxShape*)(vw->RayShape);

		FMatrix LocalWheelRot = MakeWheelRelRotMatrix( LocalToWorld(), Mesh->LocalToWorld, vw->Steer * (((FLOAT)PI)/180.f) );
		check( LocalWheelRot.Determinant() >= 0.f );

		FMatrix CapsuleTM = LocalWheelRot;
		CapsuleTM.SetOrigin( vw->WheelPosition );

		NxMat34 nCapsuleTM = U2NTransform(CapsuleTM);

		RayShape->setLocalPose(nCapsuleTM);

		// APPLY WHEEL FORCES

		// Find wheel center in world space.
		FVector WheelCenter = Mesh->LocalToWorld.TransformFVector( vw->WheelPosition );
		NxVec3 nWheelCenter = U2NPosition(WheelCenter);

		// Find wheel roll direction vector. This is the 'X' axis of the capsule transform.
		FVector WheelDir = Mesh->LocalToWorld.TransformNormal( LocalWheelRot.GetAxis(0) );
		NxVec3 nWheelDir = U2NVectorCopy(WheelDir);

		// Find wheel axle direction vector. This is the 'Z' axis of the capsule transform.
		FVector WheelAxle = Mesh->LocalToWorld.TransformNormal( LocalWheelRot.GetAxis(2) ); 
		NxVec3 nWheelAxle = U2NVectorCopy(WheelDir);


		GetLevel()->LineBatcher->DrawLine( WheelCenter, WheelCenter + (WheelAxle * 50), FColor(0,255,0) );
		GetLevel()->LineBatcher->DrawLine( WheelCenter, WheelCenter + (vw->DriveForce * WheelDir * 50), FColor(255,0,0) );

		// Apply drive force and chassis-torque to vehicle rigid body
		nActor->addForceAtPos( vw->DriveForce * nWheelDir, nWheelCenter );
		//nActor->addTorque( nWheelAxle * vw->ChassisTorque );

		// Update wheel material properties. JTODO: Probably don't want to do this every frame normally...
		NxMaterial* WheelMaterial = GNovodexSDK->getMaterial(vw->WheelMaterialIndex);
		NxSpringDesc* SpringDesc = (NxSpringDesc*)WheelMaterial->programData;

		SpringDesc->spring = vw->SuspensionStiffness;
		SpringDesc->damper = vw->SuspensionDamping;
		SpringDesc->targetValue = vw->SuspensionBias;

		WheelMaterial->restitution = vw->Restitution;
		WheelMaterial->staticFriction = vw->LatFrictionStatic;
		WheelMaterial->dynamicFriction = vw->LatFrictionDynamic;
		WheelMaterial->staticFrictionV = 0.f;
		WheelMaterial->dynamicFrictionV = 0.0f;


		// UPDATE WHEEL GRAPHICS 

		vw->CurrentRotation += (vw->SpinVel * DeltaTime * 65535/(2*PI));

		FLOAT SteerRot = (vw->Steer/360.0) * 65535.0f;

		FRotator SteerRotator;
		if(vw->BoneSteerAxis == AXIS_X)
			SteerRotator = FRotator(0, 0, SteerRot);
		else if(vw->BoneSteerAxis == AXIS_Y)
			SteerRotator = FRotator(SteerRot, 0, 0);
		else if(vw->BoneSteerAxis == AXIS_Z)
			SteerRotator = FRotator(0, SteerRot, 0);

		FRotator RollRotator;
		if(vw->BoneRollAxis == AXIS_X)
			RollRotator = FRotator(0, 0, vw->CurrentRotation);
		else if(vw->BoneRollAxis == AXIS_Y)
			RollRotator = FRotator(-vw->CurrentRotation, 0, 0);
		else if(vw->BoneRollAxis == AXIS_Z)
			RollRotator = FRotator(0, -vw->CurrentRotation, 0);

		FMatrix WheelMatrix = FMatrix(FVector(0.f), RollRotator) * FMatrix(FVector(0.f), SteerRotator);
		FRotator WheelRot = WheelMatrix.Rotator();

		Mesh->SetBoneRotation(vw->BoneName, WheelRot, 0);

		FLOAT WheelRenderDisplacement = Min<FLOAT>(vw->SuspensionMaxRenderTravel, vw->SuspensionPosition);
		Mesh->SetBoneTranslation(vw->BoneName, FVector(0.f, 0.f, WheelRenderDisplacement));

#if 0
		// See if we have a support bone (and valid distance) as well.
		if( vw->SupportBoneName != NAME_None && Abs(vw->SupportPivotDistance) > 0.001f )
		{
			FLOAT Deflection = ( 65535.0f/(2.0f * (FLOAT)PI) ) * appAtan( WheelRenderDisplacement/vw->SupportPivotDistance );

			FRotator DefRot;
			if(vw->SupportBoneAxis == AXIS_X)
				DefRot = FRotator(0, 0, Deflection);
			else if(vw->SupportBoneAxis == AXIS_Y)
				DefRot = FRotator(Deflection, 0, 0);
			else
				DefRot = FRotator(0, -Deflection, 0);

			inst->SetBoneRotation( vw->SupportBoneName, DefRot, 0, 1 );
		}
#endif

		if (fabs(vw->SpinVel) < 0.01f)
			vw->SpinVel = 0.0f;
		else
			bWheelsMoving = true;

		// Reset parameters from contact information to be possibly filled in by physics engine contact callback.
		vw->bWheelOnGround = false;
		vw->TireLoad = 0.f;
		vw->SuspensionPosition = 0.f;
	}
#endif // WITH_NOVODEX


	//if (bWheelsMoving)
	{
		Mesh->WakeRigidBody();
	}
}