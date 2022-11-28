#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

#ifdef WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(URB_RadialImpulseComponent);
IMPLEMENT_CLASS(URB_Handle);
IMPLEMENT_CLASS(URB_Spring);

#ifdef WITH_NOVODEX
static void AddRadialImpulseToBody(NxActor* Actor, const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange)
{
	if (!Actor)
		return;

	NxVec3 nCOMPos = Actor->getCMassGlobalPosition();
	NxVec3 nOrigin = U2NPosition(Origin);

	NxVec3 Delta = nCOMPos - nOrigin;
	FLOAT Mag = Delta.magnitude();

	if (Mag > Radius)
		return;

	Delta.normalize();

	// Scale by U2PScale here, because units are velocity * mass. 
	FLOAT ImpulseMag = Strength * U2PScale;
	if (Falloff == RIF_Linear)
	{
		ImpulseMag *= (1.0f - (Mag / Radius));
	}

	NxVec3 nImpulse = Delta * ImpulseMag;
	if(bVelChange)
	{
		Actor->addForce(nImpulse, NX_VELOCITY_CHANGE);
	}
	else
	{
		Actor->addForce(nImpulse, NX_IMPULSE);
	}
	Actor->wakeUp();
}
#endif // WITH_NOVODEX

//////////////// PRIMITIVE COMPONENT ///////////////

/**
 *	Find a body setup for the given PrimitiveComponent.
 *	In the case of a SkeletalMesh, we use the root bone body setup (must have a physics asset).
 */
URB_BodySetup* UPrimitiveComponent::FindRBBodySetup()
{
	UStaticMeshComponent* StatComp;
	USkeletalMeshComponent* SkelComp;

	if( (StatComp = Cast<UStaticMeshComponent>(this)) != NULL )
	{
		if(StatComp->StaticMesh)
		{
			return StatComp->StaticMesh->BodySetup;
		}
	}
	else if( (SkelComp = Cast<USkeletalMeshComponent>(this)) != NULL )
	{
		if(SkelComp->SkeletalMesh && SkelComp->PhysicsAsset)
		{
			FName RootBoneName = SkelComp->SkeletalMesh->RefSkeleton(0).Name;
			INT BodyIndex = SkelComp->PhysicsAsset->FindBodyIndex(RootBoneName);
			if(BodyIndex != INDEX_NONE)
			{
				return SkelComp->PhysicsAsset->BodySetup(BodyIndex);
			}
		}
	}

	return NULL;
}


void UPrimitiveComponent::WakeRigidBody(FName BoneName)
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if (Actor)
		Actor->wakeUp();
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execWakeRigidBody( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	WakeRigidBody(BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execWakeRigidBody);

///////

void UPrimitiveComponent::SetBlockRigidBody(UBOOL bNewBlockRigidBody)
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if(Actor)
	{
		URB_BodySetup* Setup = FindRBBodySetup();
		check(Setup); // Should always have a setup if we have an NxActor!

		// Never allow collision if bNoCollision flag is set in BodySetup.
		if(Setup->bNoCollision || !bNewBlockRigidBody)
		{
			Actor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
		}
		else
		{
			Actor->clearActorFlag( NX_AF_DISABLE_COLLISION );
		}
	}
#endif

	BlockRigidBody = bNewBlockRigidBody;
}

void UPrimitiveComponent::execSetBlockRigidBody(FFrame& Stack, RESULT_DECL)
{
	P_GET_UBOOL(bNewBlockRigidBody);
	P_FINISH;
	
	SetBlockRigidBody(bNewBlockRigidBody);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetBlockRigidBody);

///////

void UPrimitiveComponent::SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent)
{
#ifdef WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if (nActor)
	{
		NxVec3 nNewVel = U2NPosition(NewVel);

		if(bAddToCurrent)
		{
			NxVec3 nOldVel = nActor->getLinearVelocity();
			nNewVel += nOldVel;
		}

		nActor->setLinearVelocity(nNewVel);
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execSetRBLinearVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewVel);
	P_GET_UBOOL_OPTX(bAddToCurrent, false);
	P_FINISH;

	SetRBLinearVelocity(NewVel, bAddToCurrent);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRBLinearVelocity);

///////

void UPrimitiveComponent::SetRBPosition(const FVector& NewPos, const FRotator& NewRot)
{
#ifdef WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if (nActor)
	{
		FMatrix NewTM(NewPos, NewRot);

		nActor->setGlobalPose( U2NTransform(NewTM) );

		// Force a physics update now for the owner, to avoid a 1-frame lag when teleporting before the graphics catches up.
		if(Owner && Owner->Physics == PHYS_RigidBody)
		{
			Owner->SyncActorToRBPhysics();
		}
	}
#endif
}

void UPrimitiveComponent::execSetRBPosition( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewPos);
	P_GET_ROTATOR(NewRot);
	P_FINISH;

	SetRBPosition(NewPos, NewRot);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRBPosition);

///////

void UPrimitiveComponent::UpdateWindForces()
{
	// Do nothing by default.

}

void UPrimitiveComponent::execUpdateWindForces( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	UpdateWindForces();

}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execUpdateWindForces);

///////

UBOOL UPrimitiveComponent::RigidBodyIsAwake(FName BoneName)
{
#ifdef WITH_NOVODEX
	NxActor* Actor = NULL;
	
	if (BoneName == NAME_None)
		Actor = GetNxActor(FName(TEXT("ANY_BONE")));
	else
		Actor = GetNxActor(BoneName);

	if (Actor && !Actor->isSleeping())
		return true;
	else
		return false;
#endif // WITH_NOVODEX

	return false;
}

void UPrimitiveComponent::execRigidBodyIsAwake( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	*(UBOOL*)Result = RigidBodyIsAwake(BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execRigidBodyIsAwake);

///////

void UPrimitiveComponent::AddImpulse(FVector Impulse, FVector Position, FName BoneName, UBOOL bVelChange)
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor(BoneName);
	if(Actor &&	Actor->isDynamic())
	{
		NxVec3 nImpulse = U2NPosition(Impulse);

		if (Position.IsZero())
		{
			if(bVelChange)
			{
				Actor->addForce(nImpulse, NX_VELOCITY_CHANGE);
			}
			else
			{
				Actor->addForce(nImpulse, NX_IMPULSE);
			}
		}
		else
		{
			NxVec3 nPosition = U2NPosition(Position);

			if(bVelChange)
			{
				Actor->addForceAtPos(nImpulse, nPosition, NX_VELOCITY_CHANGE);
			}
			else
			{
				Actor->addForceAtPos(nImpulse, nPosition, NX_IMPULSE);
			}
		}

		Actor->wakeUp();
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddImpulse( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Impulse);
	P_GET_VECTOR_OPTX(Position, FVector(0,0,0));
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_GET_UBOOL_OPTX(bVelChange, false);
	P_FINISH;

	AddImpulse(Impulse, Position, BoneName, bVelChange);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execAddImpulse);




void UPrimitiveComponent::AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange)
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if( Actor && Actor->isDynamic() )
	{
		AddRadialImpulseToBody(Actor, Origin, Radius, Strength, Falloff, bVelChange);
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddRadialImpulse( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Origin);
	P_GET_FLOAT(Radius);
	P_GET_FLOAT(Strength);
	P_GET_BYTE(Falloff);
	P_GET_UBOOL_OPTX(bVelChange,false);
	P_FINISH;

	AddRadialImpulse(Origin, Radius, Strength, Falloff, bVelChange);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execAddRadialImpulse);

void UPrimitiveComponent::AddForce(FVector Force, FVector Position, FName BoneName)
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor(BoneName);
	if(Actor &&	Actor->isDynamic())
	{
		NxVec3 nForce = U2NVectorCopy(Force);

		if(Position.IsZero())
		{
			Actor->addForce(nForce);
		}
		else
		{
			NxVec3 nPosition = U2NPosition(Position);
			Actor->addForceAtPos(nForce, nPosition);
		}

		Actor->wakeUp();
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddForce( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Force);
	P_GET_VECTOR_OPTX(Position, FVector(0,0,0));
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	AddImpulse(Force, Position, BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execAddForce);

#ifdef WITH_NOVODEX
NxActor* UPrimitiveComponent::GetNxActor(FName BoneName)
{
	if (BodyInstance)
		return BodyInstance->GetNxActor();
	else
		return NULL;
}
#endif // WITH_NOVODEX

void UPrimitiveComponent::UpdateRBKinematicData()
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor();

	if(!Actor || !Actor->readBodyFlag(NX_BF_KINEMATIC) || Actor->readBodyFlag(NX_BF_FROZEN))
	{
		return;
	}

	// Synchronize the position and orientation of the rigid body to match the actor.
	FMatrix uTM = LocalToWorld;
	uTM.RemoveScaling();

	// Synchronize the velocity of the Novodex body to the kinematically moved object.

	URB_BodySetup* Setup = FindRBBodySetup();
	check(Setup); // We shouldn't be able to HAVE an NxActor without a BodySetup.

	check(Owner);
	FVector UseVel = Owner->Velocity;
	if( !Setup->ApparentVelocity.IsZero() )
	{
		UseVel += uTM.TransformNormal(Setup->ApparentVelocity);
	}

	NxVec3 OldPos = Actor->getGlobalPosition();
	NxQuat OldQuat = Actor->getGlobalOrientationQuat();

	// Then just use setGlobalPose to place it in the position we desire, as this does not change the velocity.
	Actor->moveGlobalPose( U2NTransform(uTM) );

	NxVec3 NewPos = Actor->getGlobalPosition();
	NxQuat NewQuat = Actor->getGlobalOrientationQuat();

	// See if we moved a non-zero distance. If so - ensure body is classed as 'awake'.

	// Calculate position and orientation differences.
	NxVec3 PosDelta = OldPos - NewPos;

	NxQuat QuatDelta;
	OldQuat.conjugate();
	QuatDelta.multiply(OldQuat, NewQuat);

	// Wake the body up if anything has changed or if it is moving.
	if (PosDelta.magnitudeSquared() > 0.0f || Abs<FLOAT>(2.0f * appAcos(QuatDelta.w)) > 0.001f || Owner->Velocity.SizeSquared() > 0.0f)
	{
		Actor->wakeUp();
	}
	else
	{
		Actor->putToSleep();
	}
#endif // WITH_NOVODEX
}

//////////////// SKELETAL MESH COMPONENT ///////////////

#ifdef WITH_NOVODEX
NxActor* USkeletalMeshComponent::GetNxActor(FName BoneName)
{
	if (PhysicsAssetInstance)
	{
		if (PhysicsAssetInstance->Bodies.Num() == 0)
			return NULL;

		if (BoneName == NAME_None)
			return NULL;

		check(PhysicsAsset);

		URB_BodyInstance* BodyInstance = NULL;

		if (BoneName == FName(TEXT("ANY_BONE")))
			BodyInstance = PhysicsAssetInstance->Bodies(0);
		else
		{
			INT BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
			if (BodyIndex == INDEX_NONE)
			{
				debugf(TEXT("AActor::HGetPhysicsBody : Could not find bone: %s"), *BoneName);
				return NULL;
			}
			BodyInstance = PhysicsAssetInstance->Bodies(BodyIndex);
		}

		check(BodyInstance);
		return BodyInstance->GetNxActor();
	}
	else
	{
		return Super::GetNxActor(BoneName);
	}
}
#endif // WITH_NOVODEX

void USkeletalMeshComponent::WakeRigidBody(FName BoneName)
{
#ifdef WITH_NOVODEX
	if (BoneName == NAME_None && PhysicsAssetInstance)
	{
		check(PhysicsAsset);

		for (int i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* Actor = BodyInstance->GetNxActor();
			if (Actor)
				Actor->wakeUp();
		}
	}
	else
	{
		NxActor* Actor = GetNxActor(BoneName);
		if (Actor)
		{
			Actor->wakeUp();
		}
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::SetBlockRigidBody(UBOOL bNewBlockRigidBody)
{
#ifdef WITH_NOVODEX
	if(PhysicsAssetInstance)
	{
		check(PhysicsAsset);

		for (int i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* Actor = BodyInstance->GetNxActor();
			if (Actor)
			{
				URB_BodySetup* Setup = PhysicsAsset->BodySetup(i);

				// Never allow collision if bNoCollision flag is set in BodySetup.
				if(Setup->bNoCollision || !bNewBlockRigidBody)
				{
					Actor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
				}
				else
				{
					Actor->clearActorFlag( NX_AF_DISABLE_COLLISION );
				}			
			}
		}
	}
#endif

	BlockRigidBody = bNewBlockRigidBody;
}

void USkeletalMeshComponent::SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent)
{
#ifdef WITH_NOVODEX
	if(PhysicsAssetInstance)
	{
		for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* nActor = BodyInstance->GetNxActor();
			if (nActor)
			{
				NxVec3 nNewVel = U2NPosition(NewVel);

				if(bAddToCurrent)
				{
					NxVec3 nOldVel = nActor->getLinearVelocity();
					nNewVel += nOldVel;
				}

				nActor->setLinearVelocity(nNewVel);
			}
		}
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::UpdateWindForces()
{
#ifdef WITH_NOVODEX
	if (PhysicsAssetInstance != NULL)
	{
		check(Owner);
		check(PhysicsAsset);

		ULevel* Level = Owner->GetLevel();
		FLOAT AbsTime = Level->TimeSeconds;
		FLOAT InvWindSpeed = 1.0f / 512.0f;

		for (UINT Index = 0; Index < UINT(PhysicsAssetInstance->Bodies.Num()); Index++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(Index);
			check(BodyInstance);

			NxActor* Actor = BodyInstance->GetNxActor();
			check(Actor);

			NxVec3 nCOMPos = Actor->getCMassGlobalPosition();
			FVector COMPos = N2UPosition(nCOMPos); 

			FVector WindForce(0);

			// Accumulate forces from point wind sources.
			for (UINT SrcIndex = 0; SrcIndex < UINT(Level->WindPointSources.Num()); SrcIndex++)
			{
				UWindPointSourceComponent* Source = Level->WindPointSources(SrcIndex);
				FVector WindVector = COMPos - Source->Position;
				FLOAT Distance = WindVector.Size();
				FLOAT Attenuation = Max(1.0f - Distance / Source->Radius, 0.0f);
				WindForce += Source->Strength * Attenuation * appSin((AbsTime + Source->Phase) * Source->Frequency * 2.0f * PI + Distance) * WindVector.Normalize();
			}

			// Accumulate forces from directional wind sources.
			for (UINT SrcIndex = 0; SrcIndex < UINT(Level->WindDirectionalSources.Num()); SrcIndex++)
			{
				UWindDirectionalSourceComponent* Source = Level->WindDirectionalSources(SrcIndex);
				FLOAT Distance = Source->Direction | COMPos;
				WindForce += Source->Strength * appSin((AbsTime + Source->Phase) * Source->Frequency * 2.0f * PI + Distance) * Source->Direction.Normalize();
			}

			// Make sure the body is awake, then apply the wind force.
			if (WindForce.X != 0.0f || WindForce.Y != 0.0f || WindForce.Z != 0.0f)
			{
				Actor->wakeUp();
				NxVec3 nWindForce = U2NVectorCopy(WindForce);
				Actor->addForce(nWindForce);
			}
		}
	}
#endif // WITH_NOVODEX
}

// Start up actual articulated physics for this 
void USkeletalMeshComponent::InitArticulated()
{
#ifdef WITH_NOVODEX
	if(!PhysicsAsset)
	{
		debugf(TEXT("USkeletalMeshComponent::InitArticulated : No PhysicsAsset defined (%s)"), GetName());
		return;
	}

	if(!Owner)
	{
		debugf(TEXT("USkeletalMeshComponent::InitArticulated : Component has no Owner! (%s)"), GetName());
		return;
	}


	// If we dont have an instance at all for this asset, use the default one.
	if(!PhysicsAssetInstance)
	{
		PhysicsAssetInstance = (UPhysicsAssetInstance*)StaticConstructObject(UPhysicsAssetInstance::StaticClass(), 
			Owner->GetLevel(), NAME_None, RF_Public|RF_Transactional, PhysicsAsset->DefaultInstance);

		PhysicsAssetInstance->CollisionDisableTable = PhysicsAsset->DefaultInstance->CollisionDisableTable;
	}

	PhysicsAssetInstance->InitInstance(this, PhysicsAsset);
#endif // WITH_NOVODEX
}

// Turn off all physics and remove the instance.
void USkeletalMeshComponent::TermArticulated()
{
#ifdef WITH_NOVODEX
	if(!PhysicsAssetInstance)
		return;

	PhysicsAssetInstance->TermInstance();

	delete PhysicsAssetInstance;
	PhysicsAssetInstance = NULL;
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::UpdateRBKinematicData()
{
#ifdef WITH_NOVODEX
	check( PhysicsAsset );
	check( PhysicsAsset->BodySetup.Num() == PhysicsAssetInstance->Bodies.Num() );

	for(int i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
	{
		NxActor* Actor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
		
		if(!Actor || !Actor->readBodyFlag(NX_BF_KINEMATIC) || Actor->readBodyFlag(NX_BF_FROZEN))
			continue;

		// Set body transform based on this actors transform
		INT BoneIndex = SkeletalMesh->MatchRefBone(PhysicsAsset->BodySetup(i)->BoneName);
		check(BoneIndex != INDEX_NONE);

		FMatrix uTM = GetBoneMatrix(BoneIndex);
		uTM.RemoveScaling();

		NxMat34 nTM = U2NTransform(uTM);

		NxVec3 OldPos, NewPos, PosDelta;
		NxQuat OldQuat, NewQuat, QuatDelta;

		Actor->getGlobalPosition(OldPos);
		Actor->getGlobalOrientationQuat(OldQuat);

		Actor->setGlobalPose(nTM);

		Actor->getGlobalPosition(NewPos);
		Actor->getGlobalOrientationQuat(NewQuat);

		// Flag as awake if actually moving, or asleep if currently stationary.

		PosDelta = OldPos - NewPos;
		OldQuat.conjugate();
		QuatDelta.multiply(OldQuat, NewQuat);

		if (PosDelta.magnitudeSquared() > 0.0f || Abs<FLOAT>(20.f * appAcos(QuatDelta.w)) > 0.001f )
			Actor->wakeUp();
		else
			Actor->putToSleep();
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange)
{
#ifdef WITH_NOVODEX
	if(PhysicsAssetInstance)
	{
		for(INT i=0; i<PhysicsAssetInstance->Bodies.Num(); i++)
		{
			NxActor* Actor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
			if(Actor && Actor->isDynamic())
			{
				AddRadialImpulseToBody(Actor, Origin, Radius, Strength, Falloff, bVelChange);
			}
		}
	}
#endif // WITH_NOVODEX
}

//////////////// STATIC MESH COMPONENT ///////////////

void UStaticMeshComponent::UpdateWindForces()
{
#ifdef WITH_NOVODEX
	check(Owner);

	ULevel* Level = Owner->GetLevel();
	FLOAT AbsTime = Level->TimeSeconds;
	FLOAT InvWindSpeed = 1.0f / 512.0f;

	check(BodyInstance);

	NxActor* Actor = BodyInstance->GetNxActor();
	check(Actor);

	NxVec3 nCOMPos = Actor->getCMassGlobalPosition();
	FVector COMPos = N2UPosition(nCOMPos); 

	FVector WindForce(0);

	// Accumulate forces from point wind sources.
	for (UINT SrcIndex = 0; SrcIndex < UINT(Level->WindPointSources.Num()); SrcIndex++)
	{
		UWindPointSourceComponent* Source = Level->WindPointSources(SrcIndex);
		FVector WindVector = COMPos - Source->Position;
		FLOAT Distance = WindVector.Size();
		FLOAT Attenuation = Max(1.0f - Distance / Source->Radius, 0.0f);
		WindForce += Source->Strength * Attenuation * appSin((AbsTime + Source->Phase) * Source->Frequency * 2.0f * PI + Distance) * WindVector.Normalize();
	}

	// Accumulate forces from directional wind sources.
	for (UINT SrcIndex = 0; SrcIndex < UINT(Level->WindDirectionalSources.Num()); SrcIndex++)
	{
		UWindDirectionalSourceComponent* Source = Level->WindDirectionalSources(SrcIndex);
		FLOAT Distance = Source->Direction | COMPos;
		WindForce += Source->Strength * appSin((AbsTime + Source->Phase) * Source->Frequency * 2.0f * PI + Distance) * Source->Direction.Normalize();
	}

	// Make sure the body is awake, then apply the wind force.
	if (WindForce.X != 0.0f || WindForce.Y != 0.0f || WindForce.Z != 0.0f)
	{
		Actor->wakeUp();
		NxVec3 nWindForce = U2NVectorCopy(WindForce);
		Actor->addForce(nWindForce);
	}
#endif // WITH_NOVODEX

}

////////////////////////////////////////////////////////////////////
// URB_RadialImpulseComponent //////////////////////////////////////
////////////////////////////////////////////////////////////////////
void URB_RadialImpulseComponent::Created()
{
	Super::Created();

	if(PreviewSphere)
		PreviewSphere->SphereRadius = ImpulseRadius;
}



void URB_RadialImpulseComponent::FireImpulse()
{
	if(!Owner)
	{
		return;
	}

	FBox SphereBox( LocalToWorld.GetOrigin() - FVector(ImpulseRadius), LocalToWorld.GetOrigin() + FVector(ImpulseRadius) );

	FMemMark Mark(GMem);

	FCheckResult* first = Owner->GetLevel()->Hash->ActorOverlapCheck(GMem, Owner, SphereBox, false);
	for(FCheckResult* result = first; result; result=result->GetNext())
	{
		UPrimitiveComponent* PokeComp = Cast<UPrimitiveComponent>(result->Component);
		if(PokeComp)
		{
			PokeComp->AddRadialImpulse( LocalToWorld.GetOrigin(), ImpulseRadius, ImpulseStrength, ImpulseFalloff, bVelChange );
		}
	}

	Mark.Pop();
}

void URB_RadialImpulseComponent::execFireImpulse( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	FireImpulse();
}

////////////////////////////////////////////////////////////////////
// URB_Handle //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void URB_Handle::Created()
{
	Super::Created();

	HandleData = NULL;
}


void URB_Handle::Destroyed()
{
	if(GrabbedComponent)
		ReleaseComponent();

	check(Owner);
	check(Owner->GetLevel());

#ifdef WITH_NOVODEX
	NxScene* Scene = Owner->GetLevel()->NovodexScene;
	if (Scene && HandleData)
	{
		NxJoint* Joint = (NxJoint*) HandleData;
		Scene->releaseJoint(*Joint);
		HandleData = NULL;
	}
#endif

	Super::Destroyed();
}

/////////////// GRAB

void URB_Handle::GrabComponent(UPrimitiveComponent* InComponent, FName InBoneName, const FVector& Location, UBOOL bConstrainRotation)
{
	// If we are already holding something - drop it first.
	if(GrabbedComponent != NULL)
		ReleaseComponent();

	if(!InComponent)
		return;

#ifdef WITH_NOVODEX
	ULevel* Level = InComponent->Owner->GetLevel();
	check(Level);

	NxScene* Scene = Level->NovodexScene;
	check(Scene);

	NxActor* Actor = InComponent->GetNxActor(InBoneName);

	if (!Actor || !Actor->isDynamic() || !Level->NovodexScene)
		return;

	if (!HandleData)
	{
		NxSphericalJointDesc PosJointDesc;

		PosJointDesc.actor[0] = Actor;
		PosJointDesc.actor[1] = NULL;

		GlobalHandlePos = Location;

		NxVec3 nHandlePos = U2NPosition(GlobalHandlePos);
		
		NxMat34 GlobalPose = Actor->getGlobalPose();
		
		NxVec3 nLocalHandlePos = GlobalPose % nHandlePos;
		LocalHandlePos = N2UPosition(nLocalHandlePos);

		PosJointDesc.localAnchor[0] = nLocalHandlePos;
		PosJointDesc.localAnchor[1] = nHandlePos;

		bRotationConstrained = bConstrainRotation;
		if (bRotationConstrained)
		{
			NxVec3 nHandleAxis1 = GlobalPose.M * NxVec3(1, 0, 0);
			HandleAxis1 = N2UVectorCopy(nHandleAxis1);

			NxVec3 nHandleAxis2 = GlobalPose.M * NxVec3(0, 1, 0);
			HandleAxis2 = N2UVectorCopy(nHandleAxis2);

			NxVec3 nHandleAxis3 = GlobalPose.M * NxVec3(0, 0, 1);
			HandleAxis3 = N2UVectorCopy(nHandleAxis3);

// pv e3 hack -- orientation constraint isn't enforced in the first frame. seems to fix the jerking.
#if 0
			PosJointDesc.localNormal[0] = NxVec3(1, 0, 0);
			PosJointDesc.localNormal[1] = nHandleAxis1;

			PosJointDesc.localAxis[0] = NxVec3(0, 0, 1);
			PosJointDesc.localAxis[1] = nHandleAxis3;

			PosJointDesc.swingLimit.value = 0.0f;
			PosJointDesc.twistLimit.low.value = -0.0001f;
			PosJointDesc.twistLimit.high.value = 0.0001f;

			PosJointDesc.flags.twistLimitEnabled = true;
			PosJointDesc.flags.swingLimitEnabled = true;
			PosJointDesc.flags.projectionMode = 1;
			PosJointDesc.projectionDistance = 0.001f;
#endif
		}

		HandleData = Scene->createJoint(PosJointDesc);
	}

#endif // WITH_NOVODEX

	GrabbedComponent = InComponent;
	GrabbedBoneName = InBoneName;
}

void URB_Handle::execGrabComponent( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UPrimitiveComponent, InComponent);
	P_GET_NAME(InBoneName);
	P_GET_VECTOR(Location);
	P_GET_UBOOL(bConstrainRotation);
	P_FINISH;

	GrabComponent(InComponent, InBoneName, Location, bConstrainRotation);
}

/////////////// RELEASE

// APE_TODO: When an actor is destroyed, use AgRB::getJoints or something to find RB_Handles and release them first.

void URB_Handle::ReleaseComponent()
{
#ifdef WITH_NOVODEX
	check(GrabbedComponent);
	check(GrabbedComponent->Owner);

	ULevel* Level = GrabbedComponent->Owner->GetLevel();
	check(Level);

	NxScene* Scene = Level->NovodexScene;
	if (Scene && HandleData)
	{
		NxJoint* PosJoint = (NxJoint*) HandleData;
		Scene->releaseJoint(*PosJoint);
		HandleData = NULL;
	}

	bRotationConstrained = false;

	GrabbedComponent = NULL;
	GrabbedBoneName = NAME_None;
#endif // WITH_NOVODEX
}

void URB_Handle::execReleaseComponent( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	ReleaseComponent();
}

/////////////// SETLOCATION

void URB_Handle::SetLocation(FVector NewLocation)
{
	if(!HandleData)
		return;

#ifdef WITH_NOVODEX
	NxActor* Actor = GrabbedComponent->GetNxActor(GrabbedBoneName);
	if (!Actor)
		return;

	NxScene* Scene = Owner->GetLevel()->NovodexScene;
	if (!Scene)
		return;

	NxJoint* PosJoint = (NxJoint*) HandleData;
	Scene->releaseJoint(*PosJoint);

	GlobalHandlePos = NewLocation;

	NxVec3 NewHandlePos = U2NPosition(GlobalHandlePos);

	NxSphericalJointDesc PosJointDesc;

	PosJointDesc.actor[0] = Actor;
	PosJointDesc.actor[1] = NULL;

	NxVec3 nLocalHandlePos = U2NPosition(LocalHandlePos);

	PosJointDesc.localAnchor[0] = nLocalHandlePos;
	PosJointDesc.localAnchor[1] = NewHandlePos;

	if (bRotationConstrained)
	{
		NxVec3 nHandleAxis1 = U2NVectorCopy(HandleAxis1);
		NxVec3 nHandleAxis2 = U2NVectorCopy(HandleAxis2);
		NxVec3 nHandleAxis3 = U2NVectorCopy(HandleAxis3);

		PosJointDesc.localNormal[0] = NxVec3(1, 0, 0);
		PosJointDesc.localNormal[1] = nHandleAxis1;

		PosJointDesc.localAxis[0] = NxVec3(0, 0, 1);
		PosJointDesc.localAxis[1] = nHandleAxis3;

		PosJointDesc.swingLimit.value = 0.0f;
		PosJointDesc.twistLimit.low.value = -0.0001f;
		PosJointDesc.twistLimit.high.value = 0.0001f;

		PosJointDesc.flags = NX_SJF_TWIST_LIMIT_ENABLED | NX_SJF_SWING_LIMIT_ENABLED;
		PosJointDesc.projectionMode = NX_JPM_POINT_MINDIST;
		PosJointDesc.projectionDistance = 0.0001f;
	}

	HandleData = Scene->createJoint(PosJointDesc);
#endif // WITH_NOVODEX
}

void URB_Handle::execSetLocation( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR( NewLocation );
	P_FINISH;

	SetLocation( NewLocation );
}

void URB_Handle::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);

#ifdef WITH_NOVODEX
	if (!HandleData || !GrabbedComponent)
		return;

	NxActor* Actor = GrabbedComponent->GetNxActor(GrabbedBoneName);
	ULevel* Level = GrabbedComponent->Owner->GetLevel();
	check(Level);
	check(Level->LineBatcher);

	// Draw 'rubber band'
	NxVec3 nLocalHandlePos = U2NPosition(LocalHandlePos);

	NxMat34 ActorPose = Actor->getGlobalPose();
	NxVec3 nEndPos = ActorPose * nLocalHandlePos;

	FVector EndPos = N2UPosition(nEndPos);

	Level->LineBatcher->DrawLine(GlobalHandlePos, EndPos, FColor(255, 0, 0));
#endif // WITH_NOVODEX
}

/////////////// SETORIENTATION

void URB_Handle::SetOrientation(FQuat NewOrientation)
{
#ifdef WITH_NOVODEX
	if(!HandleData || !GrabbedComponent)
		return;

	NxActor* Actor = GrabbedComponent->GetNxActor(GrabbedBoneName);
	if (!Actor)
		return;

	if (bRotationConstrained)
	{
		HandleAxis1 = NewOrientation.RotateVector(FVector(1, 0, 0));
		HandleAxis2 = NewOrientation.RotateVector(FVector(0, 1, 0));
		HandleAxis3 = NewOrientation.RotateVector(FVector(0, 0, 1));

		SetLocation(GlobalHandlePos);
	}
#endif // WITH_NOVODEX
}

void URB_Handle::execSetOrientation( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT( FQuat, NewOrientation );
	P_FINISH;

	SetOrientation( NewOrientation );
}

////////////////////////////////////////////////////////////////////
// URB_Spring //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

/** 
 *	Create a spring between the 2 supplied Components (and possibly bones within those Components in the case of a SkeletalMesh).
 *
 *	@param	InComponent1	Component to attach to one end of the spring.
 *	@param	InBoneName1		If InComponent1 is a SkeletalMeshComponent, this specified the bone within the mesh to attach the spring to.
 *	@param	Position1		Position (in world space) to attach spring to body of InComponent1.
 *	@param	InComponent2	Component to attach other end of spring to.
 *	@param	InBoneName2		If InComponent2 is a SkeletalMeshComponent, this specified the bone within the mesh to attach the spring to.
 *	@param	Position2		Position (in world space) to attach spring to body of InComponent2.
 */
void URB_Spring::SetComponents(UPrimitiveComponent* InComponent1, FName InBoneName1, FVector Position1, UPrimitiveComponent* InComponent2, FName InBoneName2, FVector Position2)
{
#ifdef WITH_NOVODEX
	if(!Owner)
	{
		return;
	}

	ULevel* Level = Owner->GetLevel();
	check(Level);

	NxScene* Scene = Level->NovodexScene;
	check(Scene);

	Clear();

	Component1 = InComponent1;
	BoneName1 = InBoneName1;
	Component2 = InComponent2;
	BoneName2 = InBoneName2;

	MinBodyMass = BIG_NUMBER;
	NxActor* nActor1 = NULL;
	if(InComponent1)
	{
		nActor1 = InComponent1->GetNxActor(InBoneName1);
		if(nActor1 && nActor1->isDynamic())
		{
			MinBodyMass = ::Min<FLOAT>(MinBodyMass, nActor1->getMass());
		}
	}

	NxActor* nActor2 = NULL;
	if(InComponent2)
	{
		nActor2 = InComponent2->GetNxActor(InBoneName2);
		if(nActor2 && nActor2->isDynamic())
		{
			MinBodyMass = ::Min<FLOAT>(MinBodyMass, nActor2->getMass());
		}
	}

	// Can't make spring between 2 NULL actors.
	if(!nActor1 && !nActor2)
	{
		return;
	}

	// At least one actor must be dynamic.
	UBOOL bHaveDynamic = ((nActor1 && nActor1->isDynamic()) || (nActor2 && nActor2->isDynamic()));
	if(!bHaveDynamic)
	{
		return;
	}

	// Should have got a sensible mass from at least one 
	check(MinBodyMass < BIG_NUMBER);

	//debugf( TEXT("MinBodyMass: %f"), MinBodyMass );

	// Create Novodex spring and assign Novodex NxActors.
	NxSpringAndDamperEffectorDesc SpringDesc;
	NxSpringAndDamperEffector* Spring = Scene->createSpringAndDamperEffector(SpringDesc);

	NxVec3 nPos1 = U2NPosition(Position1);
	NxVec3 nPos2 = U2NPosition(Position2);

	Spring->setBodies(nActor1, nPos1, nActor2, nPos2);

	// Save pointer to Novodex spring structure.
	SpringData = (void*)Spring;

	// Reset spring timer.
	TimeSinceActivation = 0.f;


	FLOAT UseSpringForce = SpringMaxForce;
	// If enabled, calculate 
	if(bEnableForceMassRatio)
	{
		UseSpringForce = ::Min<FLOAT>(UseSpringForce, MaxForceMassRatio * MinBodyMass);
	}
	// Scale time time-based factor
	UseSpringForce *= SpringMaxForceTimeScale.Eval( TimeSinceActivation, 1.f );

	Spring->setLinearSpring(0.f, 0.f, SpringSaturateDist, 0.f, UseSpringForce);
	Spring->setLinearDamper(-DampSaturateVel, DampSaturateVel, DampMaxForce, DampMaxForce);

	if(nActor1)
	{
		nActor1->wakeUp();
	}

	if(nActor2)
	{
		nActor2->wakeUp();
	}
#endif
}

/** 
 *	Release any spring that exists. 
 */
void URB_Spring::Clear()
{
#ifdef WITH_NOVODEX
	// Turning off the spring may have some affect, so wake the bodies now.
	if(Component1)
	{
		Component1->WakeRigidBody(BoneName1);
	}

	if(Component2)
	{
		Component2->WakeRigidBody(BoneName2);
	}

	// Clear previously sprung components.
	Component1 = NULL;
	BoneName1 = NAME_None;
	Component2 = NULL;
	BoneName2 = NAME_None;

	if(!Owner)
	{
		return;
	}

	ULevel* Level = Owner->GetLevel();
	check(Level);

	NxScene* Scene = Level->NovodexScene;
	check(Scene);

	if(SpringData)
	{
		NxSpringAndDamperEffector* Spring = (NxSpringAndDamperEffector*)SpringData;
		Scene->releaseEffector(*Spring);
		SpringData = NULL;
	}
#endif
}

/** 
 *	Each tick, update TimeSinceActivation and if there is a spring running update the parameters from the RB_Spring.
 *	This allows SpringMaxForceTimeScale to modify the strength of the spring over its lifetime.
 */
void URB_Spring::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceActivation += DeltaTime;

#ifdef WITH_NOVODEX
	if(SpringData)
	{
		NxSpringAndDamperEffector* Spring = (NxSpringAndDamperEffector*)SpringData;

		FLOAT UseSpringForce = SpringMaxForce;
		// If enabled, calculate 
		if(bEnableForceMassRatio)
		{
			UseSpringForce = ::Min<FLOAT>(UseSpringForce, MaxForceMassRatio * MinBodyMass);
		}
		// Scale time time-based factor
		UseSpringForce *= SpringMaxForceTimeScale.Eval( TimeSinceActivation, 1.f );

		Spring->setLinearSpring(0.f, 0.f, SpringSaturateDist, 0.f, UseSpringForce);
		Spring->setLinearDamper(-DampSaturateVel, DampSaturateVel, DampMaxForce, DampMaxForce);
	}
#endif

	// Ensure both bodies stay awake during spring activity.
	if(Component1)
	{
		Component1->WakeRigidBody(BoneName1);
	}

	if(Component2)
	{
		Component2->WakeRigidBody(BoneName2);
	}
}


//// Script Mirrors

void URB_Spring::execSetComponents( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UPrimitiveComponent, InComponent1);
	P_GET_NAME(InBoneName1);
	P_GET_VECTOR(InPosition1);
	P_GET_OBJECT(UPrimitiveComponent, InComponent2);
	P_GET_NAME(InBoneName2);
	P_GET_VECTOR(InPosition2);
	P_FINISH;

	SetComponents(InComponent1, InBoneName1, InPosition1, InComponent2, InBoneName2, InPosition2);
}

void URB_Spring::execClear( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	Clear();
}