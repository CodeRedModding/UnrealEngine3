/*=============================================================================
	UnSVehicle.cpp: Skeletal vehicle
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#include "EngineParticleClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(ASVehicle);
IMPLEMENT_CLASS(USVehicleWheel);
IMPLEMENT_CLASS(USVehicleSimBase);

#define SHOW_WHEEL_FORCES	(0)



UBOOL USVehicleWheel::WantsParticleComponent()
{
	return (WheelParticleSystem != NULL && WheelPSCClass != NULL);
}


#if WITH_NOVODEX

void USVehicleSimBase::SetNxWheelShapeTireForceFunctions(NxWheelShape* WheelShape, USVehicleWheel* VW, FLOAT LongGripScale, FLOAT LatGripScale)
{
	// update tire friction model
	NxTireFunctionDesc NewLongTireFunction;
	NewLongTireFunction.extremumSlip = WheelLongExtremumSlip;
	NewLongTireFunction.extremumValue = WheelLongExtremumValue * LongGripScale;
	NewLongTireFunction.asymptoteSlip = WheelLongAsymptoteSlip;
	NewLongTireFunction.asymptoteValue = WheelLongAsymptoteValue * LongGripScale;

	NxTireFunctionDesc NewLatTireFunction;
	NewLatTireFunction.extremumSlip = WheelLatExtremumSlip;
	NewLatTireFunction.extremumValue = WheelLatExtremumValue * LatGripScale;
	NewLatTireFunction.asymptoteSlip = WheelLatAsymptoteSlip;
	NewLatTireFunction.asymptoteValue = WheelLatAsymptoteValue * LatGripScale;

	// adjust tire friction model stiffness

	// first, determine if vehicle is applying handbrake
	ASVehicle * SVehicleOwner = Cast<ASVehicle>(Owner);
	UBOOL bHandbrake = SVehicleOwner ? SVehicleOwner->bOutputHandbrake : FALSE;

	if( bClampedFrictionModel )
	{
		// TireFunction stiffnessFactor not used with ClampedFrictionModel
		FLOAT LongStiffnessFactor = VW->ParkedSlipFactor;
		FLOAT LatStiffnessFactor = VW->ParkedSlipFactor;

		if ( SVehicleOwner && SVehicleOwner->bDriving )
		{
			if ( bHandbrake )
			{
				LatStiffnessFactor = VW->HandbrakeLatSlipFactor;
				LongStiffnessFactor = VW->HandbrakeLongSlipFactor;
			}
			else
			{
				LatStiffnessFactor = VW->LatSlipFactor;
				LongStiffnessFactor = VW->LongSlipFactor;
			}
		}
		NewLongTireFunction.extremumValue *= LongStiffnessFactor;
		NewLongTireFunction.asymptoteValue *= LongStiffnessFactor;
		NewLatTireFunction.extremumValue *= LatStiffnessFactor;
		NewLatTireFunction.asymptoteValue *= LatStiffnessFactor;
	}
	else
	{
		if ( SVehicleOwner && SVehicleOwner->bDriving )
		{
			if ( bHandbrake )
			{
				NewLatTireFunction.stiffnessFactor = VW->HandbrakeLatSlipFactor;
				NewLongTireFunction.stiffnessFactor = VW->HandbrakeLongSlipFactor;
			}
			else
			{
				NewLatTireFunction.stiffnessFactor = VW->LatSlipFactor;
				NewLongTireFunction.stiffnessFactor = VW->LongSlipFactor;
			}
		}
		else
		{
			NewLatTireFunction.stiffnessFactor = VW->ParkedSlipFactor;
			NewLongTireFunction.stiffnessFactor = VW->ParkedSlipFactor;
		}
	}

	WheelShape->setLongitudalTireForceFunction(NewLongTireFunction);
	WheelShape->setLateralTireForceFunction(NewLatTireFunction);
}

void USVehicleSimBase::SetNxWheelShapeParams(NxWheelShape* WheelShape, USVehicleWheel* VW, FLOAT LongGripScale, FLOAT LatGripScale)
{
	NxU32 WheelFlags = WheelShape->getWheelFlags();

	// Make sure the 'speed override' flag is set
	if(bWheelSpeedOverride)
	{
		WheelFlags |= NX_WF_AXLE_SPEED_OVERRIDE;
	}
	else
	{
		WheelFlags &= ~NX_WF_AXLE_SPEED_OVERRIDE;
	}

	// Make sure the 'clamped friction model' flag is set
	if(bClampedFrictionModel)
	{
		WheelFlags |= NX_WF_CLAMPED_FRICTION;
	}
	else
	{
		WheelFlags &= ~NX_WF_CLAMPED_FRICTION;
	}

	// Apply flags to the wheel shape.
	WheelShape->setWheelFlags(WheelFlags);

	WheelShape->setRadius(VW->WheelRadius * U2PScale);
	WheelShape->setSuspensionTravel(VW->SuspensionTravel * U2PScale);
	WheelShape->setInverseWheelMass(1.f/WheelInertia);

	NxSpringDesc SpringDesc;
	SpringDesc.spring = WheelSuspensionStiffness;
	SpringDesc.damper = WheelSuspensionDamping;
	SpringDesc.targetValue = WheelSuspensionBias;
	WheelShape->setSuspension(SpringDesc);

	SetNxWheelShapeTireForceFunctions(WheelShape, VW, LongGripScale, LatGripScale);
}

/**
 *	Calculate the orienation matrix of wheel capsule shape. 
 *	We want the raycast axis (its '-Y') to point down in Actor space.
 *	Its 'forward' axis is 'Z' and should point forwards in Actor space (X).
 *
 *	@param ActorToWorld Actor-to-world transformation.
 *	@param CompToWorld Component-to-world transformation.
 *  @param SteerAngle	Desired angle of steering, in radians.
 */
static FMatrix MakeWheelRelRotMatrix(const FMatrix& ActorToWorld, const FMatrix& CompToWorld)
{
	// Capsule directions in ACTOR space;
	FVector ActorCapX( 0,	1,	0	);
	FVector ActorCapY( 0,	0,	1	);
	FVector ActorCapZ( 1,	0,	0	);

	// Capsule directions in WORLD space.
	FVector WorldCapX = ActorToWorld.TransformNormal(ActorCapX);
	FVector WorldCapY = ActorToWorld.TransformNormal(ActorCapY);
	FVector WorldCapZ = ActorToWorld.TransformNormal(ActorCapZ);

	// Capsule directions in COMPONENT space.
	// Because root bone (chassis bone) must be at origin of SkeletalMeshComponent, this is also the transform relative to parent bone.

	FVector ComponentCapX = CompToWorld.InverseTransformNormalNoScale(WorldCapX);
	FVector ComponentCapY = CompToWorld.InverseTransformNormalNoScale(WorldCapY);
	FVector ComponentCapZ = CompToWorld.InverseTransformNormalNoScale(WorldCapZ);
	
	return FMatrix(ComponentCapX.SafeNormal(), ComponentCapY.SafeNormal(), ComponentCapZ.SafeNormal(), FVector(0.f));
}


/** 
 *	This is called from URB_BodyInstance::InitBody, and lets us add in the capsules for each wheel before creating the vehicle rigid body. 
 *	The rigid body is created at the skelmesh transform, so the wheel geometry relative transforms are just the 
 *	bone transforms (assuming wheels are direct children of chassis).
 */
void ASVehicle::ModifyNxActorDesc(NxActorDesc& ActorDesc,UPrimitiveComponent* PrimComp, const class NxGroupsMask& GroupsMask, UINT MatIndex)
{
	check(Mesh);
	check(Mesh == CollisionComponent);
	check(Mesh->SkeletalMesh);

	// If we are switching to ragdoll - bUseSingleBodyPhysics will be false - so don't do vehicle set-up stuff.
	if(!Mesh->bUseSingleBodyPhysics || PrimComp != CollisionComponent)
	{
		return;
	}

	// Warn if trying to do something silly like non-uniform scale a vehicle.
	FVector TotalScale = Mesh->Scale * DrawScale * Mesh->Scale3D * DrawScale3D;
	if( !TotalScale.IsUniform() )
	{
		debugf( TEXT("ASVehicle::ModifyNxActorDesc : Can only uniformly scale SVehicles. (%s)"), *GetName() );
		return;
	}

	// Count the number of powered wheels on the vehicle
	NumPoweredWheels = 0;
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		if(vw->bPoweredWheel)
		{
			NumPoweredWheels++;
		}
	}

	USkeletalMesh* SkelMesh = Mesh->SkeletalMesh;
	FMatrix ActorToWorld = LocalToWorld();
	ActorToWorld.RemoveScaling();

	FMatrix CompToWorld = Mesh->LocalToWorld;
	CompToWorld.RemoveScaling();

	FMatrix CompToActor = CompToWorld * ActorToWorld.Inverse();

	FMatrix LocalWheelRot = MakeWheelRelRotMatrix( ActorToWorld, CompToWorld );
	check( LocalWheelRot.Determinant() >= 0.f );

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		check(vw);

		// Get location of this wheel bone in component space.
		INT BoneIndex = SkelMesh->MatchRefBone(vw->BoneName);
		if(BoneIndex == INDEX_NONE)
		{
			debugf( TEXT("ASVehicle::ModifyNxActorDesc : Bone (%s) not found in Vehicle (%s) SkeletalMesh (%s)"), *vw->BoneName.ToString(), *GetName(), *SkelMesh->GetName() );
			check(BoneIndex != INDEX_NONE);
		}
		
		// Get wheel bone transform relative to parent. Parent should be root, which should be at same location as SkeletalMeshComponent.
		// Add offset in actor space. Scale by drawscale.
		vw->WheelPosition = TotalScale * (SkelMesh->RefSkeleton(BoneIndex).BonePos.Position + vw->BoneOffset);

		// Origin of wheel shape is start of ray - so move up by suspension travel.
		vw->WheelPosition += FVector(0, 0, vw->SuspensionTravel);
		
		FMatrix CapsuleTM = LocalWheelRot;
		CapsuleTM.SetOrigin( vw->WheelPosition );

		NxWheelShapeDesc* WheelShapeDesc = new NxWheelShapeDesc;

		NxMat34 nCapsuleTM = U2NTransform(CapsuleTM);

		WheelShapeDesc->localPose = nCapsuleTM;

		WheelShapeDesc->suspensionTravel = vw->SuspensionTravel * U2PScale;

		if (bUseSuspensionAxis)
		{
			WheelShapeDesc->wheelFlags = NX_WF_UNSCALED_SPRING_BEHAVIOR | NX_WF_WHEEL_AXIS_CONTACT_NORMAL;
		}
		else
		{
			WheelShapeDesc->wheelFlags = NX_WF_UNSCALED_SPRING_BEHAVIOR;
		}

		FRBCollisionChannelContainer TempChannels = Mesh->RBCollideWithChannels;

		// Make sure 'hover wheels' collide with water.
		if(vw->bHoverWheel)
		{
			TempChannels.SetChannel(RBCC_Water, TRUE);
		}

		// Set to collide with vehicles or not
		TempChannels.SetChannel(RBCC_Vehicle, vw->bCollidesVehicles);

		// Set to collide with pawns or not
		TempChannels.SetChannel(RBCC_Pawn, vw->bCollidesPawns);

		// Don't let things request collision _with_ the vehicle wheel.
		NxGroupsMask NewMask = CreateGroupsMask(RBCC_Nothing, &TempChannels);
		WheelShapeDesc->groupsMask = NewMask;

		WheelShapeDesc->userData = vw;

		// hardware scene support - using SW scene for vehicles
		NxScene* nScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
		check(nScene);

		ActorDesc.shapes.push_back( WheelShapeDesc );

		// If we want particles for this wheel, create the ParticleSystemComponent here.
		// We don't actually do the initialisation stuff here - that should happen the first time the component is updated.
		if (vw->WheelParticleComp == NULL && vw->WantsParticleComponent())
		{
			vw->WheelParticleComp = ConstructObject<UParticleSystemComponent>(vw->WheelPSCClass, this);
			if (vw->WheelParticleComp != NULL)
			{
				// Set the component's template to the desired ParticleSystem
				vw->WheelParticleComp->Template = vw->WheelParticleSystem;

				// Set the 'relative translation' in the Component to be at the point where the wheel touches the ground, in the actor reference frame.
				vw->WheelParticleComp->Translation = CompToActor.TransformFVector(vw->WheelPosition) - FVector(0.f, 0.f, vw->WheelRadius);
				vw->WheelParticleComp->Translation /= TotalScale;

				Components.AddItem(vw->WheelParticleComp);
			}
		}
	}	
}

void ASVehicle::OnRigidBodyCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData)
{
	Super::OnRigidBodyCollision(MyInfo, OtherInfo, RigidCollisionData);

	//debugf(TEXT("CHASSIS COLLISION: %s"), *GetName());

	bChassisTouchingGround = TRUE;
}

void ASVehicle::PostInitRigidBody(NxActor* nActor, NxActorDesc& ActorDesc, UPrimitiveComponent* PrimComp)
{
	// If this isn't the vehicle mesh itself don't re-init.
	if(PrimComp != Mesh)
	{
		return;
	}
	// If we are switching to ragdoll - bUseSingleBodyPhysics will be false - so don't do vehicle set-up stuff.
	if(!Mesh->bUseSingleBodyPhysics)
	{
		return;
	}

	// From the NxActor, iterate over shapes to find ones that point to USVehicleWheels, and store a pointer to the NxShape.
	// We'll need that later for changing the direction for steering.

	check(Mesh);
	check(Mesh == CollisionComponent);
	check(Mesh->SkeletalMesh);

	INT NumShapes = nActor->getNbShapes();
	check( NumShapes == ActorDesc.shapes.size() );

	NxShape *const * Shapes = nActor->getShapes();

	for(INT i=0; i<NumShapes; i++)
	{
		// This assumes that the only NxShapes with UserData are vehicle wheels.. should probably be more generic...
		NxShape* nShape = Shapes[i];
		if(nShape->userData)
		{
			USVehicleWheel* vw = (USVehicleWheel*)(nShape->userData);
			check(vw);
			check(vw->WheelShape == NULL); // Should not have a wheel shape already.
			vw->WheelShape = nShape;

			// Clean up the capsule shape we added in ModifyNxActorDesc
			delete ActorDesc.shapes[i];
			ActorDesc.shapes[i] = NULL;
		}
	}

	// Check we got a shape for every wheel.
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		check(vw);
		NxWheelShape* WheelShape = vw->GetNxWheelShape();
		check(WheelShape);

		// Set the various tire params on the new WheelShape.
		if(SimObj)
		{
			SimObj->SetNxWheelShapeParams(WheelShape, vw);
		}
	}

	FVector TotalScale(1.f);
	TotalScale = DrawScale * Mesh->Scale * Mesh->Scale3D * DrawScale3D;

	nActor->setCMassOffsetLocalPosition( U2NPosition(COMOffset * TotalScale.X) );
	
	// Inertia Tensor scaling
	if (InertiaTensorMultiplier.X != 1.0f || InertiaTensorMultiplier.Y != 1.0f || InertiaTensorMultiplier.Z != 1.0f)
	{
		NxVec3 nInertiaTensor = nActor->getMassSpaceInertiaTensor();

		nInertiaTensor.arrayMultiply(nInertiaTensor, NxVec3(InertiaTensorMultiplier.X, InertiaTensorMultiplier.Y, InertiaTensorMultiplier.Z));
		nActor->setMassSpaceInertiaTensor(nInertiaTensor);

		nInertiaTensor = nActor->getMassSpaceInertiaTensor();
	}

	// Stay-upright constraint
	if (bStayUpright)
	{
		check(StayUprightConstraintInstance);

		StayUprightConstraintSetup->PriAxis1 = FVector(0,0,1);
		StayUprightConstraintSetup->SecAxis1 = FVector(0,1,0);

		StayUprightConstraintSetup->PriAxis2 = FVector(0,0,1);
		StayUprightConstraintSetup->SecAxis2 = FVector(0,1,0);

		StayUprightConstraintSetup->Swing1LimitAngle = StayUprightRollResistAngle;
		StayUprightConstraintSetup->Swing2LimitAngle = StayUprightPitchResistAngle;
		StayUprightConstraintSetup->SwingLimitStiffness = StayUprightStiffness;
		StayUprightConstraintSetup->SwingLimitDamping = StayUprightDamping;

		StayUprightConstraintInstance->InitConstraint(NULL, this->CollisionComponent, StayUprightConstraintSetup, 1.0f, this, NULL, FALSE);
	}

	nActor->setMaxAngularVelocity(U2Rad * MaxAngularVelocity);
}

void ASVehicle::PreTermRigidBody(NxActor* nActor)
{
	// Return all the materials created for the wheels to the UnusedMaterials array so they can be re-used.
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		check(vw);

		if(GWorld && GWorld->RBPhysScene && vw->WheelMaterialIndex != 0)
		{
			// Return this material to the 'unused material index' pool.
			GWorld->RBPhysScene->UnusedMaterials.AddItem(vw->WheelMaterialIndex);

			vw->WheelMaterialIndex = 0;
		}

		// Clear the WheelShape pointer.
		vw->WheelShape = NULL;
	}
}

/**
 *	Shut down physics engine information for this vehicle.
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the constraint is in that scene.
 */
void ASVehicle::TermRBPhys(FRBPhysScene* Scene)
{
	// Terminate stay-upright constraint
	if (StayUprightConstraintInstance)
	{
		StayUprightConstraintInstance->TermConstraint(Scene, FALSE);
	}

	Super::TermRBPhys(Scene);
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
	if(NewPhysics == PHYS_RigidBody || NewPhysics == PHYS_None || NewPhysics == PHYS_Interpolating)
	{
		Super::setPhysics(NewPhysics, NewFloor, NewFloorV);
	}
}

/** So that full ticking is done on client. */
void ASVehicle::TickAuthoritative( FLOAT DeltaSeconds )
{
	// SVehicles should always be in PHYS_RigidBody or PHYS_None
	check(Physics == PHYS_RigidBody || Physics == PHYS_None || Physics == PHYS_Interpolating); 

	eventTick(DeltaSeconds);
	ProcessState( DeltaSeconds );
	UpdateTimers(DeltaSeconds );

	// Update LifeSpan.
	if( LifeSpan!=0.0f )
	{
		LifeSpan -= DeltaSeconds;
		if( LifeSpan <= 0.0001f )
		{
			GWorld->DestroyActor( this );
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
		// force a quick net update
		bForceNetUpdate = TRUE;
	}
}

/** So that full ticking is done on client. */
void ASVehicle::TickSimulated( FLOAT DeltaSeconds )
{
	TickAuthoritative(DeltaSeconds);
}

void ASVehicle::physRigidBody(FLOAT DeltaTime)
{
	// If on server, pack physics state to be sent to client.
	if(Role == ROLE_Authority)
	{
		VehiclePackRBState();
	}
	// If on client, update physics from server's version if there is new info.
	else
	{
		VehicleUnpackRBState();
	}

	// Do usual rigid body stuff. Will move position of Actor.
	Super::physRigidBody(DeltaTime);

	// if we don't have a valid Mesh, it isn't our CollisionComponent, or we're not using single body physics, skip the vehicle stuff
	if (Mesh == NULL || Mesh != CollisionComponent || !Mesh->bUseSingleBodyPhysics)
	{
		return;
	}

#if WITH_NOVODEX
	// Get Novodex rigid body.
	NxActor* nActor = Mesh->GetNxActor();
	if(!nActor)
	{
		return;
	}

	/////////////// UPDATE VEHICLE OUTPUT INFORMATION /////////////// 	

	FVector TotalScale = DrawScale * Mesh->Scale * Mesh->Scale3D * DrawScale3D;

	FMatrix ActorToWorld = LocalToWorld();
	ActorToWorld.RemoveScaling();

	FMatrix CompToWorld = Mesh->LocalToWorld;
	CompToWorld.RemoveScaling();

	FMatrix CompToActor = CompToWorld * ActorToWorld.Inverse();

	FVector WorldForward = ActorToWorld.GetAxis(0);
	FVector WorldRight = ActorToWorld.GetAxis(1);
	FVector WorldUp = ActorToWorld.GetAxis(2);

	bIsInverted = WorldUp.Z < 0.2f;
	ForwardVel = Velocity | WorldForward;

#if 0
	if (Driver != NULL)
	{
		ChartData( FString(TEXT("VehicleVel")), ForwardVel );
		//debugf( TEXT("ForwardVel: %f"), ForwardVel );
	}
#endif

	/////////////// UPDATE OUTPUT INFORMATION IN SVEHICLEWHEELS /////////////// 	

	if (Driver != NULL)
	{
		// Awaken vehicle
		check(CollisionComponent);
		CollisionComponent->WakeRigidBody();
	}

	UBOOL bVehicleIsSleeping = nActor->isSleeping();
	//if (!bVehicleIsSleeping)
	//	debugf(TEXT("%s is Awake!"), *GetName());
	//else
	//	debugf(TEXT("%s is SLEEPING!!"), *GetName());
	bVehicleOnGround = FALSE;

	// Read back information about the wheels from Novodex into the SVehicleWheel object.
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		check(vw);
		NxWheelShape* WheelShape = vw->GetNxWheelShape();
		check(WheelShape);

		vw->SpinVel = WheelShape->getAxleSpeed();

		NxWheelContactData ContactData;
		NxShape* ContactShape = WheelShape->getContact(ContactData);
		UBOOL bHaveContact = (ContactShape != NULL);		

		// If we have a non-NULL shape, we are contact with the ground.		
		if(bHaveContact)
		{
			bVehicleOnGround = TRUE;

			// We look to see if this shape is in the 'water' channel. Make a GroupsMask for water and compare.
			// We store the objects channel in bits0
			NxGroupsMask ShapeMask = ContactShape->getGroupsMask();
			NxGroupsMask WaterMask = CreateGroupsMask(RBCC_Water, NULL);
			bVehicleOnWater = ((ShapeMask.bits0 & WaterMask.bits0) != 0);

			vw->bWheelOnGround = TRUE;

			NxMat34 nShapeTM = WheelShape->getGlobalPose();

			// Transform contact position into ref frame of wheel shape.
			NxVec3 nLocalContactPos = nShapeTM % ContactData.contactPoint;

			// Find distance in -Y direction of contact from origin.
			FLOAT ContactFromOrigin = P2UScale * -nLocalContactPos.y;

			// Amount to move suspension
			vw->DesiredSuspensionPosition = (vw->SuspensionTravel + vw->WheelRadius) - ContactFromOrigin;

			// Read back contact data
			vw->LatSlipAngle = ContactData.lateralSlip;
			vw->LongSlipRatio = ContactData.longitudalSlip;
			vw->ContactNormal = N2UVectorCopy(ContactData.contactNormal);
			vw->LongDirection = N2UVectorCopy(ContactData.longitudalDirection);
			vw->LatDirection = N2UVectorCopy(ContactData.lateralDirection);
			vw->ContactForce = ContactData.contactForce;
			vw->LongImpulse = ContactData.longitudalImpulse;
			vw->LatImpulse = ContactData.lateralImpulse;

			// Get the object the wheel is in contact with
			NxActor* nHitActor = &ContactShape->getActor();
			if(nHitActor && nHitActor->userData)
			{
				URB_BodyInstance* ContactBI = (URB_BodyInstance*)nHitActor->userData;
				if(ContactBI->OwnerComponent)
				{
					AActor* ContactActor = ContactBI->OwnerComponent->GetOwner();
					// If its not static or world geometry, call the event on it.
					if(ContactActor && !ContactActor->bWorldGeometry && !ContactActor->IsStatic())
					{
						ContactActor->eventOnRanOver(this, ContactBI->OwnerComponent, i);
					}
				}
			}
		}
		else
		{
			// If we are not sleeping
			if(!bVehicleIsSleeping)
			{
				vw->bWheelOnGround = FALSE;

				vw->DesiredSuspensionPosition = 0.f;

				vw->LatSlipAngle = 0.f;
				vw->LongSlipRatio = 0.f;
			}
		}
	}

	// Update TimeOffGround.
	if(bVehicleOnGround)
	{
		TimeOffGround = 0.f;
	}
	else
	{
		TimeOffGround += DeltaTime;
	}

	/////////////// HANDLE INPUT /////////////// 	

	// If on the server - we work out OutputGas, OutputBrake etc, and pack them to be sent to the client.
	if(Role == ROLE_Authority)
	{
		// make sure inputs are clamped
		Throttle = Clamp(Throttle, -1.f, 1.f);
		Steering = Clamp(Steering, -1.f, 1.f); 
		Rise = Clamp(Rise, -1.f, 1.f);

		// This will update the 'Output' variables
		if(SimObj)
		{
			SimObj->ProcessCarInput(this);
		}

		// This sends them to the client
		VState.ServerBrake = FloatToRangeByte(OutputBrake);
		VState.ServerGas = FloatToRangeByte(OutputGas);
		VState.ServerSteering = FloatToRangeByte(OutputSteering);
		VState.ServerRise = FloatToRangeByte(OutputRise);
		VState.bServerHandbrake = bOutputHandbrake;
		VState.ServerView = ((DriverViewYaw & 65535) << 16) + (DriverViewPitch & 65535);
	}
	// If on client, we get OutputGas etc from the server.
	else	
	{
		OutputBrake = RangeByteToFloat(VState.ServerBrake);
		OutputGas = RangeByteToFloat(VState.ServerGas);
		OutputSteering = RangeByteToFloat(VState.ServerSteering);
		OutputRise = RangeByteToFloat(VState.ServerRise);
		bOutputHandbrake = VState.bServerHandbrake;
		DriverViewPitch	= (VState.ServerView & 65535);
		DriverViewYaw = (VState.ServerView >> 16);
	}

	//debugf( TEXT("Gas: %f Brake: %f Steering: %f"), OutputGas, OutputBrake, OutputSteering );

	/////////////// DO ENGINE MODEL & UPDATE WHEEL FORCES /////////////// 	
	if(SimObj && !bVehicleIsSleeping)
	{	
		SimObj->UpdateVehicle(this, DeltaTime);
	}
	
	///////////////// APPLY WHEEL FORCES AND UPDATE GRAPHICS /////////////// 	

	UBOOL bWheelsMoving = FALSE;
	FLOAT TotalSlipVel = 0.0f;				// Accumulate the wheels' LongSlipVel for squealing sound.

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);
		check(vw);
		NxWheelShape* WheelShape = vw->GetNxWheelShape();
		check(WheelShape);

		// Set properties on Novodex wheel
		WheelShape->setSteerAngle( vw->Steer * (((FLOAT)PI)/180.f) );

#if !FINAL_RELEASE
		if(Abs(vw->MotorTorque) > 1000.f || appIsNaN(vw->MotorTorque))
		{
			debugf(TEXT("Large/Invalid MotorTorque! %f (in %s)"), vw->MotorTorque, *GetName());
		}
#endif // FINAL_RELEASE

		WheelShape->setMotorTorque( vw->MotorTorque );
		WheelShape->setBrakeTorque( vw->BrakeTorque );

		// Apply torque back to chassis
		if( Abs(vw->ChassisTorque) > KINDA_SMALL_NUMBER )
		{
			NxVec3 nWheelAxle = U2NVectorCopy(WorldRight);
			nActor->addTorque( nWheelAxle * vw->ChassisTorque );
		}

		// UPDATE WHEEL GRAPHICS 

		// Update rotation based on current spin velocity and timestep
		vw->CurrentRotation += (vw->SpinVel * DeltaTime * (180.f/(FLOAT)PI));
		vw->CurrentRotation = appFmod( vw->CurrentRotation, 360.f ); // Unwind the rotation...

		// Update suspension position.
		FLOAT DeltaSuspension = vw->DesiredSuspensionPosition - vw->SuspensionPosition;
		FLOAT MaxSuspensionMove = vw->SuspensionSpeed * DeltaTime;

		if(DeltaSuspension > vw->SuspensionTravel * HeavySuspensionShiftPercent)
		{
			eventSuspensionHeavyShift(DeltaSuspension);
		}

		if (Abs(DeltaSuspension) < MaxSuspensionMove || vw->SuspensionSpeed == 0)
		{
			vw->SuspensionPosition = vw->DesiredSuspensionPosition;
		}
		else
		{
			if (DeltaSuspension > 0.0f)
			{
				vw->SuspensionPosition += MaxSuspensionMove;
			}
			else
			{
				vw->SuspensionPosition -= MaxSuspensionMove;
			}
		}

		// See if we have a SkelControl for this wheel, and update it if so.
		if(vw->WheelControl)
		{
			vw->WheelControl->UpdateWheelControl( (vw->SuspensionPosition/DrawScale) + vw->BoneOffset.Z, vw->CurrentRotation, vw->Steer );
		}

		// If we have a particle component for this wheel, update it now as well.
		if(vw->WheelParticleComp)
		{
			FLOAT UseSlipVel = 0.f;
			if(vw->bWheelOnGround)
			{
				UseSlipVel = (vw->SpinVel * vw->WheelRadius - ForwardVel);
			}

			// Update position of particle system to always be at base of tire (point where it hits the ground).
			vw->WheelParticleComp->Translation = CompToActor.TransformFVector(vw->WheelPosition) - FVector(0.f, 0.f, vw->SuspensionTravel + vw->WheelRadius - vw->SuspensionPosition);
			vw->WheelParticleComp->Translation /= TotalScale;

			// Let SVehicle set params, in case it wants to do more
			SetWheelEffectParams(vw, UseSlipVel);
		}

		// If wheel is very slow - clamp to zero.
		if (Abs(vw->SpinVel) < 0.01f)
		{
			//WheelShape->setAxleSpeed(0.f);
		}
		// If wheel is moving, set flag on vehicle.
		else
		{
			bWheelsMoving = TRUE;
		}

		/////////////////////
		// Do a squealing sound; volume-modulate squealing sound based on wheels' slip ratio.
		if (vw->bWheelOnGround)
		{

			if (SquealSound)
			{
				if ( Abs(vw->LongSlipRatio) > SquealThreshold || Abs(vw->LatSlipAngle) > SquealLatThreshold)
				{
					SquealSound->SetFloatParameter(FName(TEXT("VolumeModulationParam")), (Abs(vw->LongSlipRatio)>Abs(vw->LatSlipAngle*LatAngleVolumeMult))?Abs(vw->LongSlipRatio):Abs(vw->LatSlipAngle*LatAngleVolumeMult));
					if (!vw->bIsSquealing)
					{
						vw->bIsSquealing = true;
						SquealSound->Play();
					}
				}
				else
				{
					if (vw->bIsSquealing)
					{
						vw->bIsSquealing = false;
						SquealSound->Stop();
					}
				}
			}
		}
	}

	// Make sure vehicle is 'awake' if the wheels are still spinning.
	if(bWheelsMoving)
	{
		Mesh->WakeRigidBody();
	}

	/////////////// APPLY UPRIGHTING FORCES ///////////////
	
	if ( bIsUprighting )
	{
		if ( WorldInfo->TimeSeconds - UprightStartTime > UprightTime )
		{
			bIsUprighting = false;
		}
		else
		{
			const FVector UprightingLift( 0.f, 0.f, UprightLiftStrength );
			addForce(nActor, U2NVectorCopy( UprightingLift ) );
			FVector FlipTorque = ActorToWorld.GetAxis(0) * UprightTorqueStrength;
			if ( bFlipRight )
				FlipTorque *= -1.f;
			nActor->addTorque( U2NVectorCopy(FlipTorque) );
		}
	}

	/////////////// UPDATE VEHICLE SOUNDS ///////////////

	/////////////////////
	// Do a revving sound; pitch-modulate engine sound based on RPM.

	if (EngineSound != NULL && SimObj != NULL)
	{
		EngineSound->SetFloatParameter( FName(TEXT("PitchModulationParam")), SimObj->GetEngineOutput( this ) );
	}

	/////////////////////////////////////////////////////

	// Reset vehicle parameters set from contact information - will be possibly filled in by physics engine contact callback.
	bWasChassisTouchingGroundLastTick = bChassisTouchingGround;
	if (!bVehicleIsSleeping)
		bChassisTouchingGround = false;

	// Clamp velocity to MaxSpeed
	NxReal nMaxSpeed = U2PScale * MaxSpeed;
	NxVec3 nLinVel = nActor->getLinearVelocity();
	if (nLinVel.magnitudeSquared() > nMaxSpeed * nMaxSpeed)
	{
		nLinVel.normalize();
		nLinVel.setMagnitude(nMaxSpeed);
		setLinearVelocity(nActor,nLinVel);
	}
#endif // WITH_NOVODEX
}


void ASVehicle::VehiclePackRBState()
{
	UBOOL bSuccess = GetCurrentRBState(VState.RBState);
	if(bSuccess)
	{
		VState.RBState.bNewData |= UCONST_RB_NeedsUpdate;
	}
}

void ASVehicle::VehicleUnpackRBState()
{
	if( (VState.RBState.bNewData & UCONST_RB_NeedsUpdate) == UCONST_RB_NeedsUpdate )
	{
		FVector OutDeltaPos;
		ApplyNewRBState(VState.RBState, &AngErrorAccumulator,OutDeltaPos);
		VState.RBState.bNewData = UCONST_RB_None;
	}
}

/** Do sync of wheel params when we edit the sim stuff with EditActor. */
void ASVehicle::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Do nothing in the editor - no physics running that needs to be updated.
	if( GWorld->HasBegunPlay() && !IsTemplate())
	{
#if WITH_NOVODEX
		// Update Novodex wheel properties.
		if(SimObj)
		{
			for(INT i=0; i<Wheels.Num(); i++)
			{
				USVehicleWheel* vw = Wheels(i);
				check(vw);
				NxWheelShape* WheelShape = vw->GetNxWheelShape();
				check(WheelShape);

				SimObj->SetNxWheelShapeParams(WheelShape, vw);
			}
		}

		// Update center-of-mass
		NxActor* nActor = CollisionComponent->GetNxActor();
		if(nActor)
		{
			FVector TotalScale = DrawScale * CollisionComponent->Scale * CollisionComponent->Scale3D * DrawScale3D;
			nActor->setCMassOffsetLocalPosition( U2NPosition(COMOffset * TotalScale.X) );
		}
#endif
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ASVehicle::AddForce(FVector Force)
{
#if WITH_NOVODEX
	if(Force.SizeSquared() < 0.001f * 0.001f)
	{
		return;
	}

	check(!Force.ContainsNaN());

	NxActor* nActor = Mesh->GetNxActor();
	if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
	{
		addForce(nActor, U2NVectorCopy(Force) );
	}
#endif
}

void ASVehicle::AddImpulse(FVector Impulse)
{
#if WITH_NOVODEX
	check(!Impulse.ContainsNaN());

	NxActor* nActor = Mesh->GetNxActor();
	if(!nActor)
	{
		return;
	}
	Mesh->AddImpulse(Impulse);

#endif
}

void ASVehicle::AddTorque(FVector Torque)
{
	if(Torque.SizeSquared() < 0.001f * 0.001f)
	{
		return;
	}

	check(!Torque.ContainsNaN());

#if WITH_NOVODEX

	NxActor* nActor = Mesh->GetNxActor();
	if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
	{
		nActor->addTorque( U2NVectorCopy(Torque) );
	}
#endif
}

UBOOL ASVehicle::IsSleeping()
{
	UBOOL Retval = TRUE;

#if WITH_NOVODEX
	NxActor* nActor = Mesh->GetNxActor();
	if(!nActor)
	{
		Retval = TRUE;
	}
	else
	{
		Retval = nActor->isSleeping();
	}
#endif

	return Retval;
}

/** HasWheelsOnGround()
returns true if any of vehicles wheels are currently in contact with the ground (wheel has bWheelOnGround==true)
*/
UBOOL ASVehicle::HasWheelsOnGround() 
{ 
	for ( INT i=0; i<Wheels.Num(); i++ )
	{
		if ( Wheels(i)->bWheelOnGround )
			return true;
	}
	return false; 
}

void ASVehicle::SetWheelCollision(INT WheelNum, UBOOL bCollision)
{
	if ( WheelNum >= Wheels.Num() )
		return;

#if WITH_NOVODEX
	NxWheelShape* WheelShape = Wheels(WheelNum)->GetNxWheelShape();
	check(WheelShape);
	WheelShape->setFlag(NX_SF_DISABLE_COLLISION, !bCollision);
#endif
}

/** 
 *	Utility for switching the vehicle from a single body to an articulated ragdoll-like one given a new mesh and physics asset. 
 *	ActorMove is an extra translation applied to Actor during the transition, which can be useful to avoid ragdoll mesh penetrating into the ground.
 */
void ASVehicle::InitVehicleRagdoll(USkeletalMesh* RagdollMesh, UPhysicsAsset* RagdollPhysAsset, FVector ActorMove, UBOOL bClearAnimTree)
{
	if(bDeleteMe)
	{
		debugf( TEXT("InitVehicleRagdoll (%s): SVehicle is deleted!"), *GetName() );
		return;
	}

	if(!RagdollMesh || !RagdollPhysAsset)
	{
		debugf( TEXT("InitVehicleRagdoll (%s): Invalid RagdollMesh or RagdollPhysAsset."), *GetName() );
		return;
	}

	// Because root bone might be at a different point in the mesh, and we are using 'discard root translation',
	// the car mesh can pop. So what we do is look at the difference in root bone location between the meshes and
	// translate the vehicle to compensate.
	FVector OldRootPos(0,0,0);
	if(Mesh->SkeletalMesh)
	{
		OldRootPos = Mesh->SkeletalMesh->RefSkeleton(0).BonePos.Position;
	}	

	// Walk to base chain adding up world space velocities
	FVector SetVel = Velocity;
	AActor* BaseActor = Base;
	while(BaseActor)
	{
		SetVel += Base->Velocity;
		BaseActor = BaseActor->Base;
	}

	FVector NewRootPos = RagdollMesh->RefSkeleton(0).BonePos.Position;
	FVector DeltaPos = Mesh->LocalToWorld.TransformNormal(NewRootPos - OldRootPos);

	// Shut down single-body cat physics
	Mesh->TermComponentRBPhys(NULL);

	// Change mesh and asset
	if(bClearAnimTree)
	{
		Mesh->SetAnimTreeTemplate(NULL);
	}

	Mesh->SetSkeletalMesh(RagdollMesh, TRUE);
	Mesh->SetPhysicsAsset(RagdollPhysAsset);

	Mesh->bUseSingleBodyPhysics = FALSE;
	Mesh->PhysicsWeight = 1.0;
	Mesh->bIgnoreControllers = TRUE;
	Mesh->bForceDiscardRootMotion = FALSE;
	Mesh->bHasPhysicsAssetInstance = TRUE;

	// Add on user-supplied offset
	DeltaPos += ActorMove;

	// Move the actor
	FCheckResult Hit;
	GWorld->MoveActor(this, DeltaPos, Rotation, 0, Hit);

	// Initialise the ragdoll physics
	if(Mesh->IsAttached())
	{
		Mesh->InitComponentRBPhys(FALSE);
	}

	// And make sure its awake
	Mesh->WakeRigidBody();

	// Set velocity
	Mesh->SetRBLinearVelocity(SetVel, FALSE);
}

/**
  *  Used by some vehicles to limit their maximum velocity
  *	 @PARAM InForce is the force being applied to this vehicle from USVehicleSimBase::UpdateVehicle()
  *  @RETURN damping force 
  */
FVector ASVehicle::GetDampingForce(const FVector& InForce)
{
	return FVector(0.f,0.f,0.f);
}

/** Set any params on wheel particle effect */
void ASVehicle::SetWheelEffectParams(USVehicleWheel* VW, FLOAT SlipVel)
{
	// Named Instance Parameter that can be used by artists to control spawn rate etc.
	VW->WheelParticleComp->SetFloatParameter(VW->SlipParticleParamName, SlipVel);
}

