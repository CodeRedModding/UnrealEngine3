/*=============================================================================
	UnPhysActor.cpp: Actor-related rigid body physics functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 
#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineForceFieldClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineParticleClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "UnTerrain.h"
#include "UnPhysicalMaterial.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

#if WITH_NOVODEX
static FLOAT AxisSpringScale = 100.0f;
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(UKMeshProps);	
IMPLEMENT_CLASS(AKActor);
IMPLEMENT_CLASS(AKActorSpawnable);
IMPLEMENT_CLASS(AKAsset);
IMPLEMENT_CLASS(ARB_Thruster);
IMPLEMENT_CLASS(ARB_LineImpulseActor);
IMPLEMENT_CLASS(ARB_RadialImpulseActor);
IMPLEMENT_CLASS(ARB_RadialForceActor);

IMPLEMENT_CLASS(ARB_CylindricalForceActor);


DECLARE_STATS_GROUP(TEXT("PhysicsFields"),STATGROUP_PhysicsFields);
DECLARE_CYCLE_STAT(TEXT("RadialForceFieldTick"),STAT_RadialForceFieldTick,STATGROUP_PhysicsFields);
DECLARE_CYCLE_STAT(TEXT("CylindricalForceFieldTick"),STAT_CylindricalForceFieldTick,STATGROUP_PhysicsFields);

//////////////// ACTOR ///////////////

void AActor::InitRBPhys()
{
	if(bDeleteMe) 
		return;	

#if WITH_NOVODEX
	if (!GWorld->RBPhysScene)
		return;
#endif // WITH_NOVODEX

	// Iterate over all prim components in this actor, creating collision geometry for each one.
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)Components.Num(); ComponentIndex++)
	{
		UActorComponent* ActorComp = Components(ComponentIndex);
		if(ActorComp && ActorComp->IsAttached())
		{
			// If physics is PHYS_RigidBody, we only create a rigid body for the CollisionComponent.
			if(Physics == PHYS_RigidBody && ActorComp != CollisionComponent)
			{
				continue;
			}

			// Physics for object are considered 'fixed' unless Physics mode is PHYS_RigidBody
			UBOOL bFixed = true;
			if(Physics == PHYS_RigidBody)
			{
				bFixed = false;
			}

			// Initialize any physics for this component.
			ActorComp->InitComponentRBPhys(bFixed);
		}
	}
}

/** 
 *	Iterate over Actor calling TermComponentRBPhys on each ActorComponent. 
 *	This function is called to shut down any rigid-body information for the actor, when the actor is destroyed or when
 *	the physics scene is being torn down.
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the physics is in that scene.
 */
void AActor::TermRBPhys(FRBPhysScene* Scene)
{
	// Iterate over all prim components in this actor, creating collision geometry for each one.
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)Components.Num(); ComponentIndex++)
	{
		UActorComponent* ActorComp = Components(ComponentIndex);
		if(ActorComp)
		{
			ActorComp->TermComponentRBPhys(Scene);
		}
	}
}

#if WITH_NOVODEX
/** 
 *	Update the Actor to match the rigid-body physics of its CollisionComponent (if there are any). 
 */
void AActor::SyncActorToRBPhysics()
{
	if(!CollisionComponent)
	{
		debugf(TEXT("AActor::SyncActorToRBPhysics (%s) : No CollisionComponent."), *GetName());
		return;
	}

	// Get the RB_BodyInstance we are going to use as the basis for the Actor position.
	URB_BodyInstance* BodyInstance = NULL;
	FMatrix ComponentTM = FMatrix::Identity;

	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(CollisionComponent);
	if(SkelComp && !SkelComp->bUseSingleBodyPhysics)
	{
		// If we don't want to move the actor to match the root bone, or the physics is asleep, just return now doing nothing.
		if(!SkelComp->bSyncActorLocationToRootRigidBody || SkelComp->FramesPhysicsAsleep > 5)
		{
			Velocity = FVector(0.f);
			AngularVelocity = FVector(0.f);
			return;
		}

		// If there is no asset instance, we can't update from anything.
		UPhysicsAssetInstance* AssetInst = SkelComp->PhysicsAssetInstance;
		if(AssetInst)
		{
			if(AssetInst->RootBodyIndex != INDEX_NONE)
			{
				BodyInstance = AssetInst->Bodies(AssetInst->RootBodyIndex);
				if(BodyInstance->IsValidBodyInstance())
				{
					// For the skeletal case, we just move the origin of the actor, but don't rotate it.
					// JTODO: Would rotating the Actor be bad?
					FMatrix RootBoneTM = BodyInstance->GetUnrealWorldTM();
					ComponentTM = FTranslationMatrix( RootBoneTM.GetOrigin() );
				}
				else
				{
					debugf(NAME_Warning, TEXT("USkeletalMeshComponent::SyncActorToRBPhysics(): no PhysicsAssetInstance for %s with skeletalmesh: %s"), *GetFullName(), SkelComp->SkeletalMesh?*SkelComp->SkeletalMesh->GetFullName():TEXT("NULL"));
					BodyInstance = NULL;
				}
			}
			else
			{
				debugf(NAME_Warning, TEXT("USkeletalMeshComponent::SyncActorToRBPhysics(): no PhysicsAssetInstance for %s with skeletalmesh: %s"), *GetFullName(), SkelComp->SkeletalMesh?*SkelComp->SkeletalMesh->GetFullName():TEXT("NULL"));
			}
		}
		else
		{
			debugf(NAME_Warning, TEXT("USkeletalMeshComponent::SyncActorToRBPhysics(): no PhysicsAssetInstance for %s with skeletalmesh: %s"), *GetFullName(), SkelComp->SkeletalMesh?*SkelComp->SkeletalMesh->GetFullName():TEXT("NULL"));
		}
	}
	else
	{
		if(CollisionComponent->BodyInstance && CollisionComponent->BodyInstance->IsValidBodyInstance())
		{
			BodyInstance = CollisionComponent->BodyInstance;
			ComponentTM = BodyInstance->GetUnrealWorldTM();
		}
#if !FINAL_RELEASE && !NO_LOGGING
		else
		{
			FString SpecificName = TEXT( "No StaticMeshComponent Assigned" );
			UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(CollisionComponent);
			if( Comp != NULL )
			{
				SpecificName = Comp->StaticMesh->GetFullName();
			}

			debugf( TEXT("AActor::SyncActorToRBPhysics: Invalid or Missing BodyInstance in CollisionComponent. (check to make certain your object actually has a collision volume) for: %s  CollisionComp: %s  %s DetailedInfo: %s"), *GetFullName(), *SpecificName, *CollisionComponent->GetName() , *GetDetailedInfo() );
		}
#endif //!FINAL_RELEASE
	}

	// If we could not find a BodyInstance, do not move Actor.
	if( !BodyInstance )
	{
		return;
	}

	// Update actor Velocity variable to that of rigid body. Might be used by other gameplay stuff.
	BodyInstance->PreviousVelocity = BodyInstance->Velocity;
	BodyInstance->Velocity = BodyInstance->GetUnrealWorldVelocity();
	Velocity = BodyInstance->Velocity;
	AngularVelocity = BodyInstance->GetUnrealWorldAngularVelocity();

	// Now we have to work out where to put the Actor to achieve the desired transform for the component.

	// First find the current Actor-to-Component transform
	FMatrix ActorTM = LocalToWorld();
	FMatrix RelTM = CollisionComponent->LocalToWorld * ActorTM.Inverse();
	
	// Then apply the inverse of this to the new Component TM to get the new Actor TM.
	FMatrix NewTM = RelTM.Inverse() * ComponentTM;

	FVector NewLocation = NewTM.GetOrigin();
	FVector MoveBy = NewLocation - Location;

	CheckStillInWorld();
	if (bDeleteMe || Physics != PHYS_RigidBody)
	{
		return;
	}

	// If desired, see if this object is tunneling through something else (eg falling out of world)
	if(bPhysRigidBodyOutOfWorldCheck)
	{
		FCheckResult Hit(1.f);
		UBOOL bHitWorld = !GWorld->SingleLineCheck(Hit, this, NewLocation, Location, TRACE_World);
		if(bHitWorld)
		{
			debugf(TEXT("PhysRigidBodyOutOfWorldCheck:  %s -> %s"), *GetName(), *Hit.Actor->GetName());
			eventFellOutOfWorld(WorldInfo->KillZDamageType);
			if (bDeleteMe || Physics != PHYS_RigidBody)
			{
				return;
			}
		}
	}

/*
	if(!MoveBy.IsZero())
	{
		debugf( TEXT("%s    %f %f %f -> %f %f %f"), 
			GetName(), 
			Location.X, Location.Y, Location.Z, 
			NewLocation.X, NewLocation.Y, NewLocation.Z );
	}
*/

	FRotator NewRotation = NewTM.Rotator();

	// If the new location or rotation is actually different, call MoveActor.
	//@warning: do not reference BodyInstance again after calling MoveActor() - events from the move could have made it unusable (destroying the actor, SetPhysics(), etc)
	if(bAlwaysEncroachCheck || MoveBy.SizeSquared() > 0.01f * 0.01f || NewRotation != Rotation)
	{
		FCheckResult Hit(1.0f);
		GWorld->MoveActor(this, MoveBy, NewRotation, 0, Hit);
		bIsMoving = TRUE;
	}
	// If we have just stopped moving - update all components so their PreviousLocalToWorld is the same as their LocalToWorld,
	// and so motion blur realises they have stopped moving.
	else if(bIsMoving)
	{
		ForceUpdateComponents(FALSE);
		bIsMoving = FALSE;
	}
}

void AActor::SyncActorToClothPhysics()
{
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(CollisionComponent);

#if WITH_NOVODEX && !defined(NX_DISABLE_CLOTH)
	if(SkelComp && SkelComp->bEnableClothSimulation && (SkelComp->ClothSim != NULL))
	{
		NxCloth* Cloth = (NxCloth *)SkelComp->ClothSim;

		if(!SkelComp->SkeletalMesh || !SkelComp->SkeletalMesh->bEnableClothLineChecks || Cloth->isSleeping())
		{
			return;
		}

		FindTouchingActors();

		/*TODO: do we need to modify bIsMoving? */
	}
#endif
}

#endif // WITH_NOVODEX

#if WITH_NOVODEX
/** Utility for ensuring that Vec has a magnitude no larger than MaxMag. */
static NxVec3 ClampVecMag(NxVec3& Vec, FLOAT MaxMag)
{
	if(Vec.magnitudeSquared() > MaxMag * MaxMag)
	{
		Vec *= (MaxMag / Vec.magnitude());
	}

	return Vec;
}
#endif // WITH_NOVODEX

/** Utility to add extra forces necessary for rigid-body gravity and damping to the collision component. */
void AActor::AddRBGravAndDamping()
{
#if WITH_NOVODEX
	FLOAT RigidBodyDamping = 0.f;
	FMatrix BrushMatrix;
	FVector BrushScale3D;
	if(PhysicsVolume)
	{
		// Ensure damping is always positive.
		RigidBodyDamping = ::Max(PhysicsVolume->RigidBodyDamping, 0.f);

		if(PhysicsVolume->BrushComponent)
		{
			PhysicsVolume->BrushComponent->GetTransformAndScale(BrushMatrix, BrushScale3D);
		}
	}

	// See if we are not in the default physics volume, or we have some damping in this volume.

	// Gravity for this Actor.
	FLOAT GravZ = GetGravityZ();

	// See InitWorldRBPhys - this is the PhysX default 'scene' gravity.
	FLOAT RBGravZ = GWorld->GetRBGravityZ(); 

	NxVec3 nZoneVel(0,0,0);
	if(PhysicsVolume)
	{
		nZoneVel = U2NPosition(PhysicsVolume->ZoneVelocity);
	}

	if(CollisionComponent)
	{
		// In articulated skeletal case, we have to iterate over all bodies applying the necessary force to each one.
		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(CollisionComponent);
		if(SkelComp && !SkelComp->bUseSingleBodyPhysics)
		{
			FVector Scale3D = DrawScale3D * DrawScale * SkelComp->Scale * SkelComp->Scale3D;

			if(SkelComp->PhysicsAssetInstance)
			{
				for(INT i=0; i<SkelComp->PhysicsAssetInstance->Bodies.Num(); i++)
				{
					URB_BodyInstance* BodyInst = SkelComp->PhysicsAssetInstance->Bodies(i);
					check(BodyInst);

					FLOAT const DeltaGravZ = (GravZ * BodyInst->CustomGravityFactor) - RBGravZ;
					if( Abs(DeltaGravZ) > KINDA_SMALL_NUMBER || RigidBodyDamping > 0.f )
					{
						NxVec3 nDeltaGrav = U2NPosition( FVector(0.f, 0.f, DeltaGravZ) );

						NxActor* nActor = BodyInst->GetNxActor();
						if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
						{
							UBOOL bSkipBody = FALSE;
							// If desired, see if this bone is actually within the physics volume before applying effects.
							if(PhysicsVolume && PhysicsVolume->BrushComponent && SkelComp->bPerBoneVolumeEffects)
							{
								// We need to get the AABB for this bone
								check(SkelComp->PhysicsAsset);
								URB_BodySetup* BodySetup = SkelComp->PhysicsAsset->BodySetup(i);
								check(BodySetup);

								// Use transform from physics body - graphics matrices may not be up to date
								FMatrix BodyTM = BodyInst->GetUnrealWorldTM();

								FBox BodyBox = BodySetup->AggGeom.CalcAABB( BodyTM, Scale3D );
								FVector BodyCenter, BodyExtent;
								BodyBox.GetCenterAndExtents(BodyCenter, BodyExtent);

								// Then test against the brush of the physics volume.
								FCheckResult Dummy;
								UBOOL bHit = !PhysicsVolume->BrushComponent->BrushAggGeom.PointCheck(Dummy, BrushMatrix, BrushScale3D, BodyCenter, BodyExtent);

								// If we are not overlapping the volume - skip the body.
								if(!bHit)
								{
									bSkipBody = TRUE;
								}
							}

							if(!bSkipBody)
							{
								// Apply gravity force.
								AddForceNoWake( nActor, nActor->getMass() * nDeltaGrav );

								// If we have a physics volume, apply damping between objects velocity and the zone velocity.
								if(RigidBodyDamping > 0.f)
								{
									NxVec3 nRelVel = nActor->getLinearVelocity() - nZoneVel;
									NxVec3 nDampForce = -RigidBodyDamping * nRelVel * nActor->getMass();
									AddForceZeroCheck( nActor, ClampVecMag(nDampForce, PhysicsVolume->MaxDampingForce) );
								}
							}
						}
					}
				}
			}
		}
		// In the single-body case, we just use GetNxActor to get the one body and apply the force to it.
		else
		{
			URB_BodyInstance* BodyInst = CollisionComponent->BodyInstance;
			if (BodyInst)
			{
				FLOAT const DeltaGravZ = (GravZ * BodyInst->CustomGravityFactor) - RBGravZ;
				if( Abs(DeltaGravZ) > KINDA_SMALL_NUMBER || RigidBodyDamping > 0.f )
				{
					NxVec3 nDeltaGrav = U2NPosition( FVector(0.f, 0.f, DeltaGravZ) );

					NxActor* nActor = BodyInst->GetNxActor();

					if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
					{
						AddForceNoWake( nActor, nActor->getMass() * nDeltaGrav );

						if(RigidBodyDamping > 0.f)
						{
							NxVec3 nRelVel = nActor->getLinearVelocity() - nZoneVel;
							NxVec3 nDampForce = -RigidBodyDamping * nRelVel * nActor->getMass();
							AddForceZeroCheck( nActor, ClampVecMag(nDampForce, PhysicsVolume->MaxDampingForce) );
						}
					}
				}
			}
		}
	}
#endif // WITH_NOVODEX
}

void AActor::physRigidBody(FLOAT DeltaTime)
{
	check(Physics == PHYS_RigidBody);

#if WITH_NOVODEX
	SyncActorToRBPhysics();

	// If the synching caused it to be destroyed, do nothing else.
	if(IsPendingKill())
	{
		return;
	}

	// Add grav and damping forces
	AddRBGravAndDamping();
#endif
}

void AActor::physSoftBody(FLOAT DeltaTime)
{
#if WITH_NOVODEX
	if (!bNoEncroachCheck)
	{
		SyncActorToRBPhysics();
		SyncActorToClothPhysics();
	}
#endif
}

/**
 *	Applies a RigidBodyState struct to this Actor.
 *	When we get an update for the physics, we try to do it smoothly if it is less than .._THRESHOLD.
 *	We directly fix .._AMOUNT * error. The rest is fixed by altering the velocity to correct the actor over 1.0/.._RECIPFIXTIME seconds.
 *	So if .._AMOUNT is 1, we will always just move the actor directly to its correct position (as it the error was over .._THRESHOLD
 *	If .._AMOUNT is 0, we will correct just by changing the velocity.
 *
 *	For the angular case we have 3 zones.
 *	If the error is greater than UPDATE_ANGULAR_SNAP_THRESHOLD, we immediately put it to the incoming state.
 *	If the error is less than UPDATE_ANGULAR_THRESHOLD, we do the partial correction like in the linear case
 *	If its between these values, we keep accumulating its error. If the accumulator exceeds UPDATE_ANGULAR_ACCUM_ERROR_THRESHOLD,
 *	then we snap it.
 *
 *	@param	NewState				The state we want to set the physics to.
 *	@param	AngErrorAccumulator		Accumulator for the angular error. Stored externally to this function.
 */
#define UPDATE_LINEAR_THRESHOLD_SQR				(5.0f) // Physics scale
#define UPDATE_LINEAR_AMOUNT					(0.2f)
#define UPDATE_LINEAR_RECIPFIXTIME				(1.0f)
#define UPDATE_LINEAR_VEL_THRESHOLD_SQR			(0.2f) // Physics scale

#define UPDATE_ANGULAR_THRESHOLD				(0.2f * PI) // Radians
#define UPDATE_ANGULAR_SNAP_THRESHOLD			(0.5f * PI) // Radians
#define UPDATE_ANGULAR_AMOUNT					(0.1f)
#define UPDATE_ANGULAR_RECIPFIXTIME				(1.0f)
#define UPDATE_ANGULAR_ACCUM_ERROR_THRESHOLD	(60.0f * UPDATE_ANGULAR_THRESHOLD) // Corresponds to a second at 60 Hz.


void AActor::ApplyNewRBState(const FRigidBodyState& NewState, FLOAT* AngErrorAccumulator, FVector& OutDeltaPos)
{
#if WITH_NOVODEX
	check(AngErrorAccumulator);

	if(Physics == PHYS_RigidBody)
	{		
		if(CollisionComponent != NULL)
		{
			NxActor* nActor = CollisionComponent->GetNxActor();
			if(nActor)
			{
				///////////////////////////////////
				// Handy debugging mode.
				// We draw the incoming state but don't actually apply it. Useful for checking vehicle inputs are replicating correctly.
				if (FALSE)
				{
					FVector WorldX = NewState.Quaternion.RotateVector( FVector(1,0,0) );
					GWorld->LineBatcher->DrawLine( NewState.Position, NewState.Position + (100 * WorldX), FColor(255,0,0), SDPG_World );

					FVector WorldY = NewState.Quaternion.RotateVector( FVector(0,1,0) );
					GWorld->LineBatcher->DrawLine( NewState.Position, NewState.Position + (100 * WorldY), FColor(0,255,0), SDPG_World );

					FVector WorldZ = NewState.Quaternion.RotateVector( FVector(0,0,1) );
					GWorld->LineBatcher->DrawLine( NewState.Position, NewState.Position + (100 * WorldZ), FColor(0,0,255), SDPG_World );

					return;
				}
				///////////////////////////////////

				// failure cases
				FLOAT QuatSizeSqr = NewState.Quaternion.SizeSquared();
				if( QuatSizeSqr < KINDA_SMALL_NUMBER )
				{
					debugf(NAME_Warning, TEXT("Invalid zero quaternion set for body. (%s)"), *GetName());
					return;
				}
				else if( Abs(QuatSizeSqr - 1.f) > KINDA_SMALL_NUMBER )
				{
					debugf( NAME_Warning, TEXT("Quaternion (%f %f %f %f) with non-unit magnitude detected. (%s)"), 
						NewState.Quaternion.X, NewState.Quaternion.Y, NewState.Quaternion.Z, NewState.Quaternion.W, *GetName() );
					return;
				}
				// Should always be a dynamic, non-kinematic actor.
				else if (!nActor->isDynamic())
				{
					debugf(NAME_Error, TEXT("ApplyNewRBState() called for non-dynamic actor %s"), *GetName());
					return;
				}
				else if (nActor->readBodyFlag(NX_BF_KINEMATIC))
				{
					debugf(NAME_Error, TEXT("ApplyNewRBState() called for kinematic actor %s"), *GetName());
					return;
				}

				if ( (NewState.bNewData & UCONST_RB_Sleeping) == UCONST_RB_Sleeping )
				{
					if ( !nActor->isSleeping() )
					{
						nActor->putToSleep();
					}
				}
				else 
				{
					if ( nActor->isSleeping() )
					{
						nActor->wakeUp();
					}
				}

				NxVec3 OldPos = nActor->getGlobalPosition();
				NxVec3 NewPos = U2NPosition( NewState.Position );

				NxQuat OldQuat = nActor->getGlobalOrientationQuat();
				NxQuat NewQuat = U2NQuaternion( NewState.Quaternion );

				NxVec3 OldLinVel = nActor->getLinearVelocity();
				NxVec3 NewLinVel = U2NPosition( NewState.LinVel / UCONST_RBSTATE_LINVELSCALE );

				NxVec3 OldAngVel = nActor->getAngularVelocity();
				NxVec3 NewAngVel = U2NVectorCopy( NewState.AngVel / UCONST_RBSTATE_ANGVELSCALE );

				/////// POSITION CORRECTION ///////
				// Find out how much of a correction we are making
				NxVec3 Delta = NewPos - OldPos;
				FLOAT DeltaMagSqr = Delta.magnitudeSquared();
				FLOAT BodyVelMagSqr = OldLinVel.magnitudeSquared();

				NxVec3 SetPos, FixLinVel;

				// If its a small correction and velocity is above threshold, only make a partial correction, 
				// and calculate a velocity that would fix it over 'fixTime'.
				if (DeltaMagSqr < UPDATE_LINEAR_THRESHOLD_SQR && BodyVelMagSqr > UPDATE_LINEAR_VEL_THRESHOLD_SQR && !nActor->isSleeping())
				{
					SetPos = (UPDATE_LINEAR_AMOUNT * NewPos) + ((1.f - UPDATE_LINEAR_AMOUNT) * OldPos);
					FixLinVel = (NewPos - SetPos) * UPDATE_LINEAR_RECIPFIXTIME;
				}
				// If it a big correction, or we are moving too slowly, 'snap' position to state.
				else
				{
					SetPos = NewPos;
					FixLinVel.zero();
				}

				// Get the linear correction
				OutDeltaPos = N2UPosition(SetPos - OldPos);

				// Update physics body
				nActor->setGlobalPosition(SetPos);
				setLinearVelocity(nActor, (NewLinVel + FixLinVel));

				/////// ORIENTATION CORRECTION ///////

				NxQuat InvOldQuat = OldQuat;
				InvOldQuat.conjugate();

				// Get quaternion that takes us from old to new
				NxQuat DeltaQuat = NewQuat * InvOldQuat;

				// See comment at top for info on angular corrections.
				NxVec3 FixAngVel;
				NxQuat SetQuat;

				// Convert to axis and angle
				NxVec3 DeltaAxis;
				FLOAT DeltaAng = 2.f * appAcos(DeltaQuat.w);

				FLOAT W2 = ::Clamp<FLOAT>((DeltaQuat.w*DeltaQuat.w), 0.f, 1.f); 
				const FLOAT S = appSqrt(1.f - W2);
				check(!appIsNaN(S) && appIsFinite(S));
				if (S < 0.0001f) 
				{ 
					DeltaAxis.x = 1.f;
					DeltaAxis.y = 0.f;
					DeltaAxis.z = 0.f;
				} 
				else 
				{
					DeltaAxis.x = DeltaQuat.x / S;
					DeltaAxis.y = DeltaQuat.y / S;
					DeltaAxis.z = DeltaQuat.z / S;
				}

				// If the error is small, and we are moving, try to move smoothly to it
				if (DeltaAng < UPDATE_ANGULAR_THRESHOLD && BodyVelMagSqr > UPDATE_LINEAR_VEL_THRESHOLD_SQR && !nActor->isSleeping())
				{
					SetQuat.slerp( UPDATE_ANGULAR_AMOUNT, OldQuat, NewQuat );
					DeltaAxis.normalize();
					FixAngVel = DeltaAxis * DeltaAng * (1.f - UPDATE_ANGULAR_AMOUNT) * UPDATE_ANGULAR_RECIPFIXTIME;
				}
				else
				{
					SetQuat = NewQuat;
					FixAngVel.zero();
				}

				nActor->setGlobalOrientationQuat(SetQuat);
				nActor->setAngularVelocity(NewAngVel + FixAngVel);
			}
		}
	}

#endif // WITH_NOVODEX
}



/** 
 *	Get the state of the rigid body responsible for this Actor's physics, and fill in the supplied FRigidBodyState struct based on it.
 *
 *	@return	true if we successfully found a physics-engine body and update the state structure from it.
 */
UBOOL AActor::GetCurrentRBState(FRigidBodyState& OutState)
{
#if WITH_NOVODEX
	if(Physics == PHYS_RigidBody)
	{
		if(CollisionComponent != NULL)
		{
			NxActor* nActor = CollisionComponent->GetNxActor();
			if ( nActor )
			{
				OutState.Position = N2UPosition( nActor->getGlobalPosition() );
				OutState.Quaternion = N2UQuaternion( nActor->getGlobalOrientationQuat() );
				OutState.LinVel = N2UPosition( nActor->getLinearVelocity() * UCONST_RBSTATE_LINVELSCALE );
				OutState.AngVel = N2UVectorCopy( nActor->getAngularVelocity() * UCONST_RBSTATE_ANGVELSCALE );
				OutState.bNewData =	( nActor->isSleeping() ? UCONST_RB_Sleeping : UCONST_RB_None );
				return true;
			}
		}
	}
#endif // WITH_NOVODEX
	return false;
}


///////////////// TERRAIN /////////////////

/**
 *	Special version of InitRBPhys that creates Rigid Body collision for each terrain component.
 */
void ATerrain::InitRBPhys()
{
	if(!GWorld->RBPhysScene)
		return;

	// No physics terrains!
	check(Physics != PHYS_RigidBody);

	DOUBLE StartTime = appSeconds();
		
	for(INT i=0; i<TerrainComponents.Num(); i++)
	{
		UTerrainComponent* TerrainComp = TerrainComponents(i); 
		if(TerrainComp && TerrainComp->IsAttached())
		{
			check(TerrainComp->GetOwner() == this);
			TerrainComp->InitComponentRBPhys(true);
		}
	}

	DOUBLE TotalTime = appSeconds() - StartTime;
	//debugf( TEXT("Novodex Terrain Creation (%s): %f ms"), *GetName(), TotalTime * 1000.f );

	// Initialise in the Decolayer static mesh components

	// Iterate over each decolayer
	for(INT LayerIndex = 0; LayerIndex < DecoLayers.Num(); LayerIndex++)
	{
		FTerrainDecoLayer& Layer = DecoLayers(LayerIndex);

		// The over each decoration
		for(INT DecoIndex = 0; DecoIndex < Layer.Decorations.Num(); DecoIndex++)
		{
			FTerrainDecoration& Deco = Layer.Decorations(DecoIndex);

			// Then over each instance of that decoration
			for(INT InstIndex = 0; InstIndex < Deco.Instances.Num(); InstIndex++)
			{
				FTerrainDecorationInstance& Inst = Deco.Instances(InstIndex);
				if(Inst.Component && Inst.Component->IsAttached())
				{
					Inst.Component->InitComponentRBPhys(TRUE);
				}
			}
		}
	}
}

/**	
 *	Shut down physics-engine data for the Terrain.
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the physics is in that scene. 
 */
void ATerrain::TermRBPhys(FRBPhysScene* Scene)
{
	for(INT i = 0; i < TerrainComponents.Num(); i++)
	{
		UTerrainComponent* TerrainComp = TerrainComponents(i);
		if(TerrainComp)
		{
			TerrainComp->TermComponentRBPhys(Scene);
		}
	}
}

///////////////// PAWN /////////////////

/** Create/destroy sensor body that is used to detect overlaps to push rigid body actors around. */
void APawn::SetPushesRigidBodies(UBOOL NewPushes)
{
	bPushesRigidBodies = NewPushes;

#if WITH_NOVODEX
	// Turning on (creating sensor)
	if(bPushesRigidBodies && !PhysicsPushBody)
	{
		if(CollisionComponent && CollisionComponent->IsAttached() && GWorld->RBPhysScene)
		{
			// Axis aligned bounding box.
			FTranslationMatrix CollisionTM(CollisionComponent->LocalToWorld.GetOrigin());
			NxMat34 nCollisionTM = U2NTransform(CollisionTM);

			// Create kinematic sensor actor that will move with Pawn.
			NxActorDesc KinActorDesc;
			KinActorDesc.globalPose = nCollisionTM;
			KinActorDesc.density = 1.f;
			KinActorDesc.flags |= NX_AF_DISABLE_RESPONSE;

			// We create a box that stops MaxStepHeight off the floor
			FLOAT FloorPushBoxGap = MaxStepHeight + 5.f;

			NxBoxShapeDesc BoxDesc;
			FVector Extent = GetCylinderExtent();
			Extent.X += RBPushRadius;
			Extent.Y += RBPushRadius;
			Extent.Z -= 0.5f*FloorPushBoxGap;
			BoxDesc.dimensions = U2NPosition(Extent);

			FMatrix BoxOffset = FMatrix::Identity;
			BoxOffset.M[3][2] = 0.5f*FloorPushBoxGap;
			BoxDesc.localPose = U2NTransform(BoxOffset);

			// Set collision filtering on the sensor shape.
			FRBCollisionChannelContainer CollidesWith(0);
			CollidesWith.SetChannel(RBCC_Default, TRUE);
			CollidesWith.SetChannel(RBCC_EffectPhysics, TRUE);
			CollidesWith.SetChannel(RBCC_GameplayPhysics, TRUE);
			NxGroupsMask GroupsMask = CreateGroupsMask(RBCC_Nothing, &CollidesWith);
			BoxDesc.groupsMask = GroupsMask;

			KinActorDesc.shapes.push_back(&BoxDesc);

			NxBodyDesc KinBodyDesc;
			KinBodyDesc.flags |= NX_BF_KINEMATIC;
			KinActorDesc.body = &KinBodyDesc;

			NxScene* nScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
			if(nScene)
			{
				// Create physics object for detecting collisions
				NxActor* KinActor = nScene->createActor(KinActorDesc);
				KinActor->setGroup(UNX_GROUP_NOTIFYCOLLIDE);

				// Then create an RB_BodyInstance to contain the physics sensor object
				URB_BodyInstance* BodyInstance = GWorld->InstanceRBBody(NULL);
				BodyInstance->BodyData = KinActor;
				BodyInstance->bPushBody = TRUE; // So collision notification goes to the right place
				BodyInstance->bEnableCollisionResponse = FALSE; // No physical collision with objects
				BodyInstance->OwnerComponent = Mesh; // Not really true, but need it to find this Actor inside the callback
				BodyInstance->SceneIndex = 	GWorld->RBPhysScene->NovodexSceneIndex;

				// assign pointers
				KinActor->userData = BodyInstance;
				PhysicsPushBody = BodyInstance;
			}
		}
	}
	// Turning off (destroying sensor)
	else if(!bPushesRigidBodies && PhysicsPushBody)
	{
		PhysicsPushBody->TermBody(NULL);
		PhysicsPushBody = NULL;
	}
#endif // WITH_NOVODEX
}

/** Update physics 'sensor' body as Actor moves. This is used to find overlaps with physics bodies and apply forces. */
void APawn::UpdatePushBody()
{
#if WITH_NOVODEX
	if(CollisionComponent && CollisionComponent->IsAttached() && PhysicsPushBody && PhysicsPushBody->IsValidBodyInstance())
	{
		// Axis aligned bounding box.
		FTranslationMatrix CollisionTM(CollisionComponent->LocalToWorld.GetOrigin());
		NxMat34 nCollisionTM = U2NTransform(CollisionTM);

		if(CollisionTM.ContainsNaN() || (nCollisionTM.M.determinant() < (FLOAT)KINDA_SMALL_NUMBER))
		{
			debugf(TEXT("UpdatePushBody : Invalid transform (%s)"), *GetName());
		}
		else
		{
			NxActor* KinActor = PhysicsPushBody->GetNxActor();
			if(KinActor)
			{
				KinActor->moveGlobalPose(nCollisionTM);
			}
		}

	}
#endif // WITH_NOVODEX
}

/** Called when the push body 'sensor' overlaps a physics body. Allows you to add a force to that body to move it. */
void APawn::ProcessPushNotify(const FRigidBodyCollisionInfo& PushedInfo, const TArray<FRigidBodyContactInfo>& ContactInfos)
{
	for(INT i=0; i<ContactInfos.Num(); i++)
	{
		const FRigidBodyContactInfo& Info = ContactInfos(i);
		FVector ToContact = Info.ContactPosition - Location;
		ToContact.Z = 0;
		ToContact.Normalize();

		check(PushedInfo.Component);
		PushedInfo.Component->AddForce(ToContact * RBPushStrength, Info.ContactPosition, NAME_None);

		if(FALSE)
		{
			DrawWireStar(GWorld->LineBatcher, Info.ContactPosition, 5.f, FColor(0,255,255), SDPG_World);
			GWorld->LineBatcher->DrawLine(Info.ContactPosition, Info.ContactPosition + (50.f * ToContact), FColor(0,255,255), SDPG_World);
		}
	}
}

void APawn::InitRBPhys()
{
	Super::InitRBPhys();
	SetPushesRigidBodies(bPushesRigidBodies);
}

void APawn::TermRBPhys(FRBPhysScene* Scene)
{
	Super::TermRBPhys(Scene);
	SetPushesRigidBodies(false);
}

UBOOL APawn::InitRagdoll()
{
	if (bDeleteMe)
	{
		debugf( TEXT("InitRagdoll: Pawn (%s) is deleted!"), *GetName() );
		return false;
	}
	else if (!Mesh || !Mesh->PhysicsAsset)
	{
		return false;
	}
	else if (Physics == PHYS_RigidBody && Mesh == CollisionComponent)
	{
		// we are already in ragdoll - do nothing and say we were successful
		return true;
	}
	else if (Mesh->GetOwner() != this)
	{
		debugf(TEXT("InitRagdoll: SkeletalMeshComponent.Owner (%x) is not the Pawn (%x)"), Mesh->GetOwner(), this);
		return false;
	}
	else
	{
		// if we had some other rigid body thing going on, cancel it
		if (Physics == PHYS_RigidBody)
		{
			//@note: Falling instead of None so Velocity/Acceleration don't get cleared
			setPhysics(PHYS_Falling);
		}

		// initialize physics/etc
		PreRagdollCollisionComponent = CollisionComponent;
		CollisionComponent = Mesh;
		Mesh->PhysicsWeight = 1.0f;
		Mesh->SetHasPhysicsAssetInstance(TRUE);

		// inherit velocity from moving bases
		FVector BaseLinearVel(0.f);
		if (Base && !Base->IsStatic())
		{
			BaseLinearVel = Base->Velocity;

			if (!Base->AngularVelocity.IsZero())
			{
				FVector BaseLinearVelFromAngularVel = Base->AngularVelocity ^ (Location - Base->Location);	//RelativeLocation;
				BaseLinearVel += BaseLinearVelFromAngularVel;
			}
		}

		setPhysics(PHYS_RigidBody);

		// You never want bodies fixed when in PHYS_RigidBody, otherwise you get feedback loop and character flies off!
		if(Mesh->PhysicsAssetInstance)
		{
			Mesh->PhysicsAssetInstance->SetAllBodiesFixed(FALSE);
		}

		Mesh->WakeRigidBody();

		if (!BaseLinearVel.IsZero())
		{
			Mesh->SetRBLinearVelocity(BaseLinearVel, TRUE);
		}

		return true;
	}
}

/** the opposite of InitRagdoll(); resets CollisionComponent to the default,
 * sets physics to PHYS_Falling, and calls TermArticulated() on the SkeletalMeshComponent
 * @return true on success, false if there is no Mesh, the Mesh is not in ragdoll, or we're otherwise not able to terminate the physics
 */
UBOOL APawn::TermRagdoll()
{
	if (bDeleteMe)
	{
		debugf( TEXT("TermRagdoll: Pawn (%s) is deleted!"), *GetName() );
		return false;
	}
	else if (Mesh == NULL || Mesh->PhysicsAsset == NULL || Mesh != CollisionComponent)
	{
		return false;
	}
	else
	{
		if (Mesh->GetOwner() != this)
		{
			debugf(TEXT("TermRagdoll: SkeletalMeshComponent.Owner (%x) is not the Pawn (%x)"), Mesh->GetOwner(), this);
			return false;
		}
		else
		{
			// reset physics/etc
			if (PreRagdollCollisionComponent != NULL && PreRagdollCollisionComponent->IsAttached() && PreRagdollCollisionComponent->GetOwner() == this)
			{
				CollisionComponent = PreRagdollCollisionComponent;
			}
			else
			{
				CollisionComponent = CylinderComponent;
			}
			PreRagdollCollisionComponent = NULL;
			Mesh->PhysicsWeight = 0.f;
			Mesh->SetHasPhysicsAssetInstance(FALSE);
			if (Physics == PHYS_RigidBody)
			{
				setPhysics(PHYS_Falling);
			}

			return true;
		}
	}
}

///////////////// KACTOR /////////////////

void AKActor::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);

	// If 'stay upright' is enabled, apply torque to try and achieve that
	if(bEnableStayUprightSpring)
	{
		const FVector LocalUp = LocalToWorld().GetAxis(2);

		// Cross to get vector
		const FVector Cross = FVector(0,0,1) ^ LocalUp;
		const FLOAT CrossMag = Cross.Size();

		// If non-parallel..
		if(CrossMag > KINDA_SMALL_NUMBER)
		{
			// Find angle between vectors
			FLOAT LevelAngle = appAsin(CrossMag);

			const FLOAT Dot = FVector(0,0,1) | LocalUp;
			if(Dot < 0.0f)
			{
				LevelAngle = PI - LevelAngle;
			}
			// Normalize axis
			const FVector LevelAxis = Cross / CrossMag;

			// Scale and clamp
			const FLOAT UprightTorqueMag = ::Clamp(StayUprightTorqueFactor * LevelAngle, -StayUprightMaxTorque, StayUprightMaxTorque);
			StaticMeshComponent->AddTorque(-UprightTorqueMag * LevelAxis);
		}
	}
}


void AKActor::physRigidBody(FLOAT DeltaTime)
{
	// If we are the authority - pack current physics state into struct.
	if( Role == ROLE_Authority )
	{
		const UBOOL bSuccess = GetCurrentRBState(RBState);
		if(bSuccess)
		{
			// Flag that we need updating
			RBState.bNewData |= UCONST_RB_NeedsUpdate;
			// determine if replication is now required
			if (!bNeedsRBStateReplication)
			{
				bNeedsRBStateReplication = !(RBState.bNewData & UCONST_RB_Sleeping) || (InitialLocation - Location).SizeSquared() > 1.0f;
			}
		}
	}
	// If we are a client, see if we have receieved new state. If so, apply it to the physics.
	else
	{
		// if the server last said we were asleep, but we woke up somehow, force the update to be reapplied
		// since if the wakeup call didn't happen on the server, it won't send any further updates and we'll go out of sync
		if ((RBState.bNewData & UCONST_RB_Sleeping) && CollisionComponent != NULL && CollisionComponent->RigidBodyIsAwake())
		{
			RBState.bNewData |= UCONST_RB_NeedsUpdate;
		}

		if (RBState.bNewData & UCONST_RB_NeedsUpdate)
		{
			FVector OutDeltaPos;
			ApplyNewRBState(RBState, &AngErrorAccumulator, OutDeltaPos);
			RBState.bNewData &= ~UCONST_RB_NeedsUpdate;
		}
	}

	Super::physRigidBody(DeltaTime);

	// If desired, limit how quickly the KActor can move
	if(bLimitMaxPhysicsVelocity)
	{
		const FLOAT VMag = Velocity.Size();
		if(VMag > MaxPhysicsVelocity)
		{
			const FVector NewVel = (Velocity/VMag) * MaxPhysicsVelocity;
			StaticMeshComponent->SetRBLinearVelocity(NewVel, FALSE);
		}
	}

	// Handle turning off sliding effects if we are not in contact, or we are not sliding fast enough.
	if(!bCurrentSlide && bSlideActive)
	{
		if(SlideEffectComponent)
		{
			SlideEffectComponent->DeactivateSystem();
		}

		if(SlideSoundComponent)
		{
			SlideSoundComponent->FadeOut(0.4f,0.0f);
		}

		LastSlideTime = GWorld->GetTimeSeconds();
		bSlideActive = FALSE;
	}

	// Reset 
	bCurrentSlide = FALSE;
}

/** Utility for seeing if there is ConstraintActor attached to the supplied Actor. */
static UBOOL ActorHasJoint(AActor* InActor)
{
	// Iterate over all Actors looking for any ConstraintActors that are connected to the supplied Actor.
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		ARB_ConstraintActor* ConActor = Cast<ARB_ConstraintActor>(Actor);
		if(ConActor)
		{
			if(	ConActor->ConstraintActor1 == InActor ||
				ConActor->ConstraintActor2 == InActor )
			{
				return TRUE;
			}
		}
	}

	// No constraint found.
	return FALSE;
}

#if WITH_EDITOR
void AKActor::CheckForErrors()
{
	Super::CheckForErrors();
	if ( StaticMeshComponent && StaticMeshComponent->StaticMesh ) // DynamicSMActor::CheckForErrors GWarn's for these.
	{
		if( StaticMeshComponent->StaticMesh->BodySetup == NULL )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_StaticMeshWithNullBodySetup" ), *GetName() ) ), TEXT( "StaticMeshWithNullBodySetup" ) );
		}

		if( !StaticMeshComponent->BlockRigidBody && !ActorHasJoint(this) )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KActorWithNoBlockRigidBodyAndNoJoints" ), *GetName() ) ), TEXT( "KActorWithNoBlockRigidBodyAndNoJoints" ) );
		}
		if( StaticMeshComponent->StaticMesh->bCanBecomeDynamic )
		{
			GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KActorWithNoBlockRigidBodyAndNoJoints" ), *GetName(), *GetDetailedInfo() ) ), TEXT( "KActorWithNoBlockRigidBodyAndNoJoints" ) );
		}
	}
}
#endif

/** 
 * This is only called when resetting a KActor.  It forces a resolve of the RBState
 */
void AKActor::ResolveRBState()
{
	// Get the Current RBState ignoring if it's sleeping.  Then flag this 
	// actor for replication

	if ( GetCurrentRBState(RBState) )
	{
		RBState.bNewData = UCONST_RB_NeedsUpdate;
		if ( !bWakeOnLevelStart )
		{
			RBState.bNewData |= UCONST_RB_Sleeping;
		}

		bNetDirty=true;
	}
}

/** Make AKActors be hit when using the TRACE_Mover flag. */
UBOOL AKActor::ShouldTrace(UPrimitiveComponent *Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	return (TraceFlags & TRACE_Movers) 
				&& ((TraceFlags & TRACE_OnlyProjActor) 
					? (bProjTarget || (bBlockActors && Primitive->BlockActors)) 
					: (!(TraceFlags & TRACE_Blocking) || (SourceActor && SourceActor->IsBlockedBy(this,Primitive))));
}

/** Util for getting the PhysicalMaterial applied to this KActor's StaticMesh. */
UPhysicalMaterial* AKActor::GetKActorPhysMaterial()
{
	FCheckResult Check(0.f);
	Check.Actor = this;
	Check.Component = StaticMeshComponent;

	return DetermineCorrectPhysicalMaterial(Check);
}

void AKActor::OnRigidBodyCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData)
{
	const FLOAT SlideFadeInTime=0.5f;
	Super::OnRigidBodyCollision(MyInfo, OtherInfo, RigidCollisionData);

	// Find relative velocity.
	// use pre-collision velocity of the root body, if we have it
	FVector Velocity0 = RigidCollisionData.ContactInfos(0).ContactVelocity[0];
	FVector AngularVel0 = FVector(0.f);
	if (MyInfo.Component != NULL)
	{
		URB_BodyInstance* BodyInstance = MyInfo.Component->GetRootBodyInstance();
		if(BodyInstance)
		{
			if (!BodyInstance->PreviousVelocity.IsZero())
			{
				Velocity0 = BodyInstance->PreviousVelocity;
			}
			AngularVel0 = BodyInstance->GetUnrealWorldAngularVelocity();
		}
	}
	FVector Velocity1 = RigidCollisionData.ContactInfos(0).ContactVelocity[1];
	FVector AngularVel1 = FVector(0.f);
	if (OtherInfo.Component != NULL)
	{
		URB_BodyInstance* BodyInstance = OtherInfo.Component->GetRootBodyInstance();
		if(BodyInstance)
		{
			if (!BodyInstance->PreviousVelocity.IsZero())
			{
				Velocity1 = BodyInstance->PreviousVelocity;
			}
			AngularVel1 = BodyInstance->GetUnrealWorldAngularVelocity();
		}
	}

	
	FVector RelVel = Velocity1 - Velocity0;

	// Then project along contact normal, and take magnitude.
	FLOAT ImpactVelMag = RelVel | RigidCollisionData.ContactInfos(0).ContactNormal;
	FLOAT SlideVelMag = (RelVel - (RigidCollisionData.ContactInfos(0).ContactNormal * ImpactVelMag)).Size();
	ImpactVelMag = Abs(ImpactVelMag);

	// Difference in angular velocity between contacting bodies.
	FLOAT AngularVelMag = (AngularVel1 - AngularVel0).Size() * 70.f;
	
	// If bodies collide and are rotating quickly, even if relative linear velocity is not that high, 
	// use the value from the angular velocity instead.

	//debugf(TEXT("%f"),ImpactVelMag);
	//debugf(TEXT("Angular: %f"),AngularVelMag);
	if (ImpactVelMag < AngularVelMag)
	{
		ImpactVelMag = AngularVelMag;
	}

	//debugf( TEXT("I: %f S: %f"), ImpactVelMag, SlideVelMag );

	// Impact effect
	FLOAT TimeSinceLastImpact = GWorld->GetTimeSeconds() - LastImpactTime;
	UBOOL bDidImpact = FALSE;

	// Get transform of contact relative to Actor
	FMatrix ActorToWorld = LocalToWorld();
	FMatrix WorldToActor = ActorToWorld.Inverse();
	FVector LocalContactPos = WorldToActor.TransformFVector(RigidCollisionData.ContactInfos(0).ContactPosition);
	FVector LocalContactNormal = WorldToActor.TransformNormal(RigidCollisionData.ContactInfos(0).ContactNormal);
	LocalContactNormal = LocalContactNormal.SafeNormal();

	// Make particle system point 'at' this KActor- flip normal if pair is flipped
	if(MyInfo.Actor != this)
	{
		LocalContactNormal = -LocalContactNormal;
	}

	FRotator LocalContactRot = LocalContactNormal.Rotation();

	// We don't allow impact effects to fire when sliding.
	if( (ImpactVelMag > ImpactEffectInfo.Threshold) && (TimeSinceLastImpact > ImpactEffectInfo.ReFireDelay) && !bSlideActive)
	{
		if(ImpactEffectComponent)
		{
			if( ImpactEffectComponent->IsAttached() == FALSE )
			{
				AttachComponent( ImpactEffectComponent );
			}

			ImpactEffectComponent->Translation = LocalContactPos;
			ImpactEffectComponent->Rotation = LocalContactRot;
			ImpactEffectComponent->BeginDeferredUpdateTransform();

			ImpactEffectComponent->SetFloatParameter(NAME_ImpactVel, ImpactVelMag);
			ImpactEffectComponent->ActivateSystem();
		}

		if(ImpactSoundComponent && ImpactSoundComponent2)
		{
			static UBOOL bImpactSoundOdd = TRUE; // @todo this needs to be per instance
			if(bImpactSoundOdd)
			{
				if( ImpactSoundComponent->IsAttached() == FALSE )
				{
					AttachComponent( ImpactSoundComponent );
				}

				ImpactSoundComponent->SetFloatParameter(NAME_ImpactVel, ImpactVelMag);
				ImpactSoundComponent->Play();
				bImpactSoundOdd = FALSE;
			}
			else
			{
				if( ImpactSoundComponent2->IsAttached() == FALSE )
				{
					AttachComponent( ImpactSoundComponent2 );
				}

				ImpactSoundComponent2->SetFloatParameter(NAME_ImpactVel, ImpactVelMag);
				ImpactSoundComponent2->Play();
				bImpactSoundOdd = TRUE;
			}
		}

		bDidImpact = TRUE;
		LastImpactTime = GWorld->GetTimeSeconds();
	}

	// Slide effect
	bCurrentSlide = (SlideVelMag > SlideEffectInfo.Threshold);
	FLOAT TimeSinceLastSlide = GWorld->GetTimeSeconds() - LastSlideTime;

	// If we think a slide is active, but it isn't now
	if(!bSlideActive && bCurrentSlide && (TimeSinceLastSlide > SlideEffectInfo.ReFireDelay) && !bDidImpact)
	{
		if(SlideEffectComponent)
		{
			if( SlideEffectComponent->IsAttached() == FALSE )
			{
				AttachComponent( SlideEffectComponent );
			}

			SlideEffectComponent->SetFloatParameter(NAME_SlideVel, SlideVelMag);
			SlideEffectComponent->ActivateSystem();
		}

		if(SlideSoundComponent)
		{
			if( SlideSoundComponent->IsAttached() == FALSE )
			{
				AttachComponent( SlideSoundComponent );
			}

			SlideSoundComponent->SetFloatParameter(NAME_SlideVel, SlideVelMag);
			SlideSoundComponent->FadeIn(SlideFadeInTime,1.0f);
		}

		bSlideActive = TRUE;
	}

	// Update transform of slide emitter each frame sliding is active.
	if(bSlideActive)
	{
		if(SlideEffectComponent)
		{
			if( SlideEffectComponent->IsAttached() == FALSE )
			{
				AttachComponent( SlideEffectComponent );
			}

			SlideEffectComponent->SetFloatParameter(NAME_SlideVel, SlideVelMag);
			SlideEffectComponent->Translation = LocalContactPos;
			SlideEffectComponent->Rotation = LocalContactRot;
			SlideEffectComponent->BeginDeferredUpdateTransform();
		}

		if(SlideSoundComponent)
		{
			if( SlideSoundComponent->IsAttached() == FALSE )
			{
				AttachComponent( SlideSoundComponent );
			}

			SlideSoundComponent->SetFloatParameter(NAME_SlideVel, SlideVelMag);
		}
	}

	// Turning off sliding effects is handled in physRigidBody - this function does not get called when not in contact
}

///////////////// KActorSpawnable /////////////////
void AKActorSpawnable::ResetComponents()
{
	//ADynamicSMActor
	// Don't release the static mesh nor the LightEnvironment as the SMC uses it
	//StaticMeshComponent = NULL;
	//DetachComponent(LightEnvironment);
    //LightEnvironment = NULL;
    ReplicatedMesh = NULL;
    ReplicatedMaterial0 = NULL;
	ReplicatedMaterial1 = NULL;

	//AKActor
	DetachComponent(ImpactEffectComponent);
    ImpactEffectComponent = NULL;
	DetachComponent(ImpactSoundComponent);
    ImpactSoundComponent = NULL;
	DetachComponent(ImpactSoundComponent2);
    ImpactSoundComponent2 = NULL;
	DetachComponent(SlideEffectComponent);
    SlideEffectComponent = NULL;
	DetachComponent(SlideSoundComponent);
    SlideSoundComponent = NULL;
}

/**
  * Support for scaling down to zero before destroying
  */
void AKActorSpawnable::TickSpecial( FLOAT DeltaTime )
{
	Super::TickSpecial( DeltaTime );

	// if we are scaling to zero then do so
	if( bScalingToZero )
	{
		SetDrawScale( DrawScale - DeltaTime );
		if( DrawScale < 0.02f )
		{
			bScalingToZero = FALSE;
			SetDrawScale( GetClass()->GetDefaultActor()->DrawScale );
			eventRecycleInternal();
		}
	}
}

///////////////// KASSET /////////////////

/** 
 *	Implementation of OnRigidBodyCollision callback. Will only get called if the CollisionComponent for the KActor has bNotifyRigidBodyCollision set.
 *	This function looks to see if there are any RigidBodyCollision events associated with this KActor, and calls CheckRBCollisionActor on them if so.
 *
 *	@todo At the moment we only look at the first contact in the ContactInfos array. Maybe improve this?
 */
void AActor::OnRigidBodyCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData)
{
#if 0
	if(TRUE)
	{
		FString MyName = MyInfo.Component ? FString(*MyInfo.Component->GetPathName()) : FString(TEXT(""));
		FString OtherName = OtherInfo.Component ? FString(*OtherInfo.Component->GetPathName()) : FString(TEXT(""));
		debugf( TEXT("COLLIDE! %s - %s"), *MyName, *OtherName );
	}
#endif

	checkSlow(RigidCollisionData.ContactInfos.Num() > 0);

	// Find relative velocity.
	// use pre-collision velocity of the root body, if we have it
	FVector Velocity0 = RigidCollisionData.ContactInfos(0).ContactVelocity[0];
	if (MyInfo.Component != NULL)
	{
		URB_BodyInstance* BodyInstance = MyInfo.Component->GetRootBodyInstance();
		if (BodyInstance != NULL && !BodyInstance->PreviousVelocity.IsZero())
		{
			Velocity0 = BodyInstance->PreviousVelocity;
		}
	}
	FVector Velocity1 = RigidCollisionData.ContactInfos(0).ContactVelocity[1];
	if (OtherInfo.Component != NULL)
	{
		URB_BodyInstance* BodyInstance = OtherInfo.Component->GetRootBodyInstance();
		if (BodyInstance != NULL && !BodyInstance->PreviousVelocity.IsZero())
		{
			Velocity1 = BodyInstance->PreviousVelocity;
		}
	}
	
	FVector RelVel = Velocity1 - Velocity0;

	// Then project along contact normal, and take magnitude.
	FLOAT VelMag = Abs(RelVel | RigidCollisionData.ContactInfos(0).ContactNormal); 

	// Determine if 'we' are 0 or 1. 'We' are the Originator, and the thing we hit is the Instigator.	
	
	if (ensure(MyInfo.Actor == this))
	{
		if ( MyInfo.Component != NULL )
		{
			if ( MyInfo.Actor->bWorldGeometry && !MyInfo.Component->IsAlwaysStatic() )
			{
				// looks like this is a staticmeshcomponent that can become dynamic
				UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(MyInfo.Component);

				if ( SMC && SMC->CanBecomeDynamic() )
				{
					Cast<AKActorFromStatic>(AKActorFromStatic::StaticClass()->GetDefaultActor())->MakeDynamic(SMC);
					SMC->AddImpulse(RelVel, RigidCollisionData.ContactInfos(0).ContactPosition, NAME_None, TRUE);

					// MyInfo.Actor changed, so early out
					return;
				}
			}
			if ( MyInfo.Component->ScriptRigidBodyCollisionThreshold > 0.f && VelMag >= MyInfo.Component->ScriptRigidBodyCollisionThreshold )
			{
				eventRigidBodyCollision(MyInfo.Component, OtherInfo.Component, RigidCollisionData, 0);
			}
		}

		// Search to see if we have any RigidBodyCollision events.
		for(INT Idx = 0; Idx < GeneratedEvents.Num(); Idx++)
		{
			USeqEvent_RigidBodyCollision *CollideEvent = Cast<USeqEvent_RigidBodyCollision>(GeneratedEvents(Idx));
			if (CollideEvent != NULL)
			{
				CollideEvent->CheckRBCollisionActivate(MyInfo, OtherInfo, RigidCollisionData.ContactInfos, VelMag);
			}
		}
	}
}

///////////////// USeqEvent_RigidBodyCollision /////////////////

/**
 *	This is called when a physics collision has occured, and a Kismet event is desired.
 *	It will check the velocity at the impact, and call CheckActivate if its above the MinCollisionVelocity threshold.
 */
void USeqEvent_RigidBodyCollision::CheckRBCollisionActivate(const FRigidBodyCollisionInfo& OriginatorInfo, 
															const FRigidBodyCollisionInfo& InstigatorInfo, 
															const TArray<FRigidBodyContactInfo>& ContactInfos,
															FLOAT VelMag)
{
	// If it is above threshold, call back CheckActive function. This should take care of re-triggering etc.
	if(VelMag > MinCollisionVelocity)
	{
		UBOOL bActivated = CheckActivate(OriginatorInfo.Actor, InstigatorInfo.Actor, FALSE);
		if(bActivated)
		{
			// Set the impact velocity output.
			TArray<FLOAT*> FloatVars;
			GetFloatVars(FloatVars,TEXT("ImpactVelocity"));
			if (FloatVars.Num() > 0)
			{
				for (INT Idx = 0; Idx < FloatVars.Num(); Idx++)
				{
					*(FloatVars(Idx)) = VelMag;
				}
			}

			// Set the impact location output.
			FVector ContactPos = (ContactInfos.Num() > 0) ? ContactInfos(0).ContactPosition : FVector(0.f, 0.f, 0.f);
			TArray<FVector*> VectorVars;
			GetVectorVars(VectorVars,TEXT("ImpactLocation"));
			if (VectorVars.Num() > 0)
			{
				for (INT Idx = 0; Idx < VectorVars.Num(); Idx++)
				{
					*(VectorVars(Idx)) = ContactPos;
				}
			}
		}
	}
}

///////////////// RB_THRUSTER /////////////////

UBOOL ARB_Thruster::Tick( FLOAT DeltaTime, ELevelTick TickType )
{
	UBOOL bTicked = Super::Tick(DeltaTime, TickType);
	if(bTicked)
	{
		// Applied force to the base, so if we don't have one, do nothing.
		if(bThrustEnabled && Base)
		{
			FMatrix ActorTM = LocalToWorld();
			FVector WorldForce = ThrustStrength * ActorTM.TransformNormal( FVector(-1,0,0) );

			// Skeletal case.
			if(BaseSkelComponent)
			{
				BaseSkelComponent->AddForce(WorldForce, Location, BaseBoneName);
			}
			// Single-body case.
			else if(Base->CollisionComponent)
			{
				Base->CollisionComponent->AddForce(WorldForce, Location);
			}
		}
	}
	return bTicked;
}

///////////////// RB_LINEIMPULSEACTOR /////////////////

/** Used to keep the arrow graphic updated with the impulse line check length. */
void ARB_LineImpulseActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Arrow->ArrowSize = ImpulseRange/48.f;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#if WITH_EDITOR
void ARB_LineImpulseActor::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 50.0f;

	const FLOAT Multiplier = ( DeltaScale.X > 0.0f || DeltaScale.Y > 0.0f || DeltaScale.Z > 0.0f ) ? 1.0f : -1.0f;
	ImpulseRange += Multiplier * ModifiedScale.Size();
	ImpulseRange = Max( 0.f, ImpulseRange );
	PostEditChange();
}
#endif

/** Do a line check and apply an impulse to anything we hit. */
void ARB_LineImpulseActor::FireLineImpulse()
{
	FMatrix ActorTM = LocalToWorld();
	FVector ImpulseDir = ActorTM.TransformNormal( FVector(1,0,0) );


	// Do line check, take the first result and apply impulse to it.
	if(bStopAtFirstHit)
	{
		FCheckResult Hit(1.f);
		UBOOL bHit = !GWorld->SingleLineCheck( Hit, this, Location + (ImpulseRange * ImpulseDir), Location, TRACE_Actors | TRACE_ComplexCollision, FVector(0.f) );
		if(bHit)
		{
			check(Hit.Component);
			Hit.Component->AddImpulse( ImpulseDir * ImpulseStrength, Hit.Location, Hit.BoneName, bVelChange );

			// See if we want to fracture stuff.
			if(bCauseFracture)
			{
				AFracturedStaticMeshActor* FracActor = Cast<AFracturedStaticMeshActor>(Hit.Actor);
				if(FracActor)
				{
					// Make struct to pass hit item
					FTraceHitInfo HitInfo = FTraceHitInfo();
					HitInfo.Item = Hit.Item;

					FracActor->eventTakeDamage(100000000, NULL, Hit.Location, ImpulseDir, NULL, HitInfo, this);
				}
			}
		}
	}
	// Do the line check, find all Actors along length of line, and apply impulse to all of them.
	else
	{
		FMemMark Mark(GMainThreadMemStack);
		FCheckResult* FirstHit = GWorld->MultiLineCheck
			(
			GMainThreadMemStack,
			Location + (ImpulseRange * ImpulseDir),
			Location,
			FVector(0,0,0),
			TRACE_Actors | TRACE_ComplexCollision,
			this
			);

		// Iterate over each thing we hit, adding an impulse to the components we hit.
		for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			check(Check->Component);
			Check->Component->AddImpulse( ImpulseDir * ImpulseStrength, Check->Location, Check->BoneName, bVelChange );

			// See if we want to fracture stuff.
			if(bCauseFracture)
			{
				AFracturedStaticMeshActor* FracActor = Cast<AFracturedStaticMeshActor>(Check->Actor);
				if(FracActor)
				{
					// Make struct to pass hit item
					FTraceHitInfo HitInfo = FTraceHitInfo();
					HitInfo.Item = Check->Item;

					FracActor->eventTakeDamage(100000000, NULL, Check->Location, ImpulseDir, NULL, HitInfo, this);
				}
			}
		}

		Mark.Pop();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void ARB_RadialImpulseActor::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
	ImpulseComponent->ImpulseRadius += Multiplier * ModifiedScale.Size();
	ImpulseComponent->ImpulseRadius = Max( 0.f, ImpulseComponent->ImpulseRadius );
	PostEditChange();
}
#endif
///////////////// ARB_RADIALFORCEACTOR /////////////////

//Note: this is taken from UBrushComponent. But unfortunatly was removed in the april QA. 
// Add back to the brush component?

static INT AddConvexVolumes(UBrushComponent* BrushComp, TArray<FConvexVolume>& ConvexVolumes, UBOOL bConvertToWorldSpace )
{
	for( INT ConvexElementIndex=0; ConvexElementIndex<BrushComp->BrushAggGeom.ConvexElems.Num(); ConvexElementIndex++ )
	{
		// Get convex volume planes for brush from physics data.
		const FKConvexElem& ConvexElement	= BrushComp->BrushAggGeom.ConvexElems(ConvexElementIndex);
		TArray<FPlane>		BrushPlanes		= ConvexElement.FacePlaneData;
		// Convert from local to world space if wanted.
		if( bConvertToWorldSpace )
		{
			for( INT PlaneIndex=0; PlaneIndex<BrushPlanes.Num(); PlaneIndex++ )
			{
				FPlane& BrushPlane = BrushPlanes(PlaneIndex);
				BrushPlane = BrushPlane.TransformBy( BrushComp->LocalToWorld );
			}
		}
		// Add convex volume.
		ConvexVolumes.AddItem( FConvexVolume( BrushPlanes ) );
	}
	return BrushComp->BrushAggGeom.ConvexElems.Num();
}



static UBOOL CheckCollisionChannels(BYTE RBChannel, const FRBCollisionChannelContainer& CollideWithChannels)
{

	//TODO: Use bitops? (endian issue?)

	switch(RBChannel)
	{
	case RBCC_Default:
		return CollideWithChannels.Default;

	case RBCC_Nothing:
		return FALSE;

	case RBCC_Pawn:
		return CollideWithChannels.Pawn;

	case RBCC_Vehicle:
		return CollideWithChannels.Vehicle;

	case RBCC_Water:
		return CollideWithChannels.Water;

	case RBCC_GameplayPhysics:
		return CollideWithChannels.GameplayPhysics;

	case RBCC_EffectPhysics:
		return CollideWithChannels.EffectPhysics;

	case RBCC_Untitled1:
		return CollideWithChannels.Untitled1;

	case RBCC_Untitled2:
		return CollideWithChannels.Untitled2;

	case RBCC_Untitled3:
		return CollideWithChannels.Untitled3;
	
	case RBCC_Untitled4:
		return CollideWithChannels.Untitled4;

	case RBCC_Cloth:
		return CollideWithChannels.Cloth;

	case RBCC_FluidDrain:
		return CollideWithChannels.FluidDrain;

	case RBCC_SoftBody:
		return CollideWithChannels.SoftBody;

	case RBCC_FracturedMeshPart:
		return CollideWithChannels.FracturedMeshPart;
	case RBCC_Clothing:
		return CollideWithChannels.Clothing;
	case RBCC_ClothingCollision:
		return CollideWithChannels.ClothingCollision;

	default:
		return TRUE;
	}
}
///////////////// ARB_RADIALFORCEACTOR /////////////////
/* Callback class used to compute a radial force field (explosion, black hole etc)  */
class FRadialForceApplicator : public FForceApplicator
{

	const FVector& Origin;
	const FLOAT Radius;
	const FLOAT Strength;

	const FLOAT SwirlStrength;
	const FLOAT SpinTorque;
	const BYTE Falloff;

	TArray<FConvexVolume> ConvexExcludeVolumes;
	TArray<FBox> ConvexExcludeBounds;

	/* Compute a cylindrical/tornado force given appropriate parameters and position/velocity. */
	inline UBOOL ComputeRadialForce(const FVector& Position, const FVector& Velocity, FVector& Result)
	{
		Result = FVector(0.0f, 0.0f, 0.0f);

		FVector Delta = Position - Origin;
		FLOAT Mag = Delta.Size();
		
		if (Mag > Radius)
			return FALSE;

		Delta.Normalize();

		// If using lienar falloff, scale with distance.
		FLOAT ForceMag = Strength;
		if (Falloff == RIF_Linear)
		{
			ForceMag *= (1.0f - (Mag / Radius));
		}

		// Apply force
		Result = Delta * ForceMag;
		
		// Get swirling force.
		const FVector Up(0.f, 0.f, 1.f);
		FVector CrossDir = Up ^ Delta;
		Result += CrossDir * SwirlStrength;

		return TRUE;

	}

	inline UBOOL IsPointExcluded(const FVector& Position, const FBox& PositionBoundingBox)
	{

		for(INT i=0; i<ConvexExcludeVolumes.Num(); i++)
		{
			if(!ConvexExcludeBounds(i).Intersect(PositionBoundingBox))
				continue;
			
			const TArray<FPlane>& Planes = ConvexExcludeVolumes(i).Planes;
			INT j;

			for(j=0; j<Planes.Num(); j++)
			{
				if(Planes(j).PlaneDot(Position) > 0.0f)
				{
					break; //outside
				}
			}

			if(j == Planes.Num())
			{
				return TRUE; // inside this volume
			}
		}

		return FALSE;
	}

public:

	FRadialForceApplicator(const FVector& InOrigin, const FLOAT InRadius, const FLOAT InStrength, const FLOAT InSwirlStrength, const FLOAT InSpinTorque,
		const BYTE InFalloff, const FBox& ForceBBox): 

		Origin(InOrigin), Radius(InRadius), Strength(InStrength), SwirlStrength(InSwirlStrength), SpinTorque(InSpinTorque), Falloff(InFalloff)
	{
		
	}

	virtual UBOOL ComputeForce(
		FVector* Positions, INT PositionStride, FLOAT PositionScale,
		FVector* Velocities, INT VelocityStride, FLOAT VelocityScale,
		FVector* OutForce, INT OutForceStride, FLOAT OutForceScale,
		FVector* OutTorque, INT OutTorqueStride, FLOAT OutTorqueScale,
		INT Count, const FBox& PositionBoundingBox )
	{
		UBOOL NonZero = FALSE;

		for(INT i = 0; i < Count; i++)
		{
			FVector Position = *Positions * PositionScale;

			if(!IsPointExcluded(Position, PositionBoundingBox))
			{
				FVector Result;
				if(ComputeRadialForce(Position, *Velocities * VelocityScale, Result))
				{
					NonZero = TRUE;
					*OutForce += Result * OutForceScale;
					if(OutTorque)
					{
						*OutTorque += FVector(0,0,SpinTorque) * OutTorqueScale;
					}
				}
			}

			Positions = (FVector *) (((BYTE *)Positions) + PositionStride);
			Velocities = (FVector *) (((BYTE *)Velocities) + VelocityStride);
			OutForce = (FVector *) (((BYTE *)OutForce) + OutForceStride);
			if(OutTorque)
			{
				OutTorque = (FVector *) (((BYTE *)OutTorque) + OutTorqueStride);
			}
		}

		return NonZero;
	}
};

#if WITH_EDITOR
void ARB_RadialForceActor::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
	ForceRadius += Multiplier * ModifiedScale.Size();
	ForceRadius = Max( 0.f, ForceRadius );
	PostEditChange();
}
#endif

/** Update the render component to match the force radius. */
void ARB_RadialForceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RenderComponent)
	{
		FComponentReattachContext ReattachContext(RenderComponent);
		RenderComponent->SphereRadius = ForceRadius;
	}
}


/** In the Tick function, apply forces to anything near this Actor. */
void ARB_RadialForceActor::TickSpecial(FLOAT DeltaSeconds)
{

	SCOPE_CYCLE_COUNTER(STAT_RadialForceFieldTick);

	Super::TickSpecial(DeltaSeconds);

	if(bForceActive)
	{
		FBoxSphereBounds BoxSphereBounds( FBox(Location - FVector(ForceRadius), Location + FVector(ForceRadius)) );

		//Create the force applicator
		FRadialForceApplicator Applicator( Location, ForceRadius, ForceStrength, SwirlStrength, SpinTorque, ForceFalloff,
			BoxSphereBounds.GetBox());

		if(bForceApplyToCloth || bForceApplyToRigidBodies)
		{
			// Query Octree to find nearby things.
			FMemMark Mark(GMainThreadMemStack);
			FCheckResult* FirstOverlap = GWorld->Hash->ActorOverlapCheck(GMainThreadMemStack, this, Location, ForceRadius);

			for(FCheckResult* result = FirstOverlap; result; result=result->GetNext())
			{
				UPrimitiveComponent* Comp = result->Component;
				if(Comp)
				{
					if(!Comp->bIgnoreRadialForce)
					{
						UBOOL bClothForce = bForceApplyToCloth && CollideWithChannels.Cloth;
						UBOOL bRigidBodyForce = bForceApplyToRigidBodies && CheckCollisionChannels(Comp->RBChannel, CollideWithChannels);

						if(bClothForce || bRigidBodyForce)
						{
							Comp->AddForceField( &Applicator, BoxSphereBounds.GetBox(), bClothForce, bRigidBodyForce);
						}
					}
				}
			}

			//Done with overlapping components.
			Mark.Pop();
		}

		if(bForceApplyToProjectiles)
		{
			//ApplyProjectileForceField(Applicator);
		}

		if (RadialForceMode == RFT_Impulse)
		{
			// Deactivate the force for future use as Impulses are only applied once!
			bForceActive = FALSE;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ARB_CYLINDRICALFORCEACTOR /////////////////

#if WITH_EDITOR
void ARB_CylindricalForceActor::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if(!ModifiedScale.IsUniform())
	{
		const FLOAT XYMultiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f ) ? 1.0f : -1.0f;
		const FLOAT ZMultiplier = ( ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

		ForceRadius += XYMultiplier * ModifiedScale.Size2D();
		ForceTopRadius += XYMultiplier * ModifiedScale.Size2D();
		ForceHeight += ZMultiplier * Abs(ModifiedScale.Z);
	}
	else
	{
		const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

		ForceRadius += Multiplier * ModifiedScale.Size();
		ForceTopRadius += Multiplier * ModifiedScale.Size();
		ForceHeight += Multiplier * ModifiedScale.Size();
	}

	ForceRadius = Max( 0.f, ForceRadius );
	ForceTopRadius = Max(0.0f, ForceTopRadius);
	ForceHeight = Max( 0.0f, ForceHeight );

	PostEditChange();
}
#endif

/** Update the render component to match the force radius. */
void ARB_CylindricalForceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RenderComponent)
	{
		{
			FComponentReattachContext ReattachContext(RenderComponent);
			RenderComponent->CylinderRadius = ForceRadius;
			RenderComponent->CylinderTopRadius = ForceTopRadius;
			RenderComponent->CylinderHeight = ForceHeight;
			RenderComponent->CylinderHeightOffset = HeightOffset;
		}
	}
}

/* Callback class used to compute a cylindrical/chopped off cone force field (tornado, pipe, fan etc)  */
class FCylindricalForceApplicator : public FForceApplicator
{
	const FVector Origin;
	const FVector UpVector;
	const FLOAT Radius;
	const FLOAT RadialStrength;
	const FLOAT RotationalStrength;
	const FLOAT LiftStrength;
	const FLOAT EscapeVelocity; 
	const FLOAT RadiusTop;
	const FLOAT LiftFalloffHeight;
	const UBOOL bSpecialRadialForce;
	TArray<FConvexVolume> ConvexExcludeVolumes;
	TArray<FBox> ConvexExcludeBounds;

	/* Compute a cylindrical/tornado force given appropriate parameters and position/velocity. */
	inline UBOOL ComputeCylindricalForce(const FVector& Position, const FVector& Velocity, FVector& Result)
	{
		Result = FVector(0.0f, 0.0f, 0.0f);

		FLOAT TornadoHeight = UpVector.Size();

		if(TornadoHeight < KINDA_SMALL_NUMBER)
			return FALSE;

		FVector UnitUpVector = UpVector / TornadoHeight;

		FLOAT BodyHeight = UnitUpVector | (Position - Origin);
		if((BodyHeight < 0.0f) || (BodyHeight > TornadoHeight))
			return FALSE;

		FVector CenterPos = BodyHeight * UnitUpVector + Origin;
		FVector CenterDir = Position - CenterPos;
		FLOAT CenterDirMag = CenterDir.Size();

		FLOAT UnitBodyHeight = BodyHeight / TornadoHeight;
		FLOAT RadiusAtBodyHeight = Lerp(Radius, RadiusTop, UnitBodyHeight);

		if(CenterDirMag > RadiusAtBodyHeight)
			return FALSE;

		if(CenterDirMag > KINDA_SMALL_NUMBER)
		{
			FVector UnitCenterDir = CenterDir / CenterDirMag;
			FVector Tangent = UnitCenterDir ^ UnitUpVector;
			FLOAT Factor = CenterDirMag / RadiusAtBodyHeight;

			Result += Tangent * ( 1.0f - Factor) * RotationalStrength;

			if (bSpecialRadialForce)
			{
				if(((Velocity | UnitCenterDir) > KINDA_SMALL_NUMBER) && (Velocity.Size() < EscapeVelocity))
				{
					Result += UnitCenterDir * Factor *  RadialStrength;
				}
			}
			else
			{
				Result += UnitCenterDir * (1.0f-Factor) *  RadialStrength;
			}

		}

		if(UnitBodyHeight > LiftFalloffHeight)
		{
			FLOAT FalloffRange = 1.0f - LiftFalloffHeight;
			FLOAT FalloffHeight = (UnitBodyHeight - LiftFalloffHeight) / FalloffRange; 

			FLOAT LiftFactor = 1.0f -  FalloffHeight;	
			Result += UnitUpVector * LiftStrength * LiftFactor;
		}
		else
		{
			Result += UnitUpVector * LiftStrength;
		}
	
		return TRUE;

	}

	inline UBOOL IsPointExcluded(const FVector& Position, const FBox& PositionBoundingBox)
	{

		for(INT i=0; i<ConvexExcludeVolumes.Num(); i++)
		{
			if(!ConvexExcludeBounds(i).Intersect(PositionBoundingBox))
				continue;
			
			const TArray<FPlane>& Planes = ConvexExcludeVolumes(i).Planes;
			INT j;

			for(j=0; j<Planes.Num(); j++)
			{
				if(Planes(j).PlaneDot(Position) > 0.0f)
				{
					break; //outside
				}
			}

			if(j == Planes.Num())
			{
				return TRUE; // inside this volume
			}
		}

		return FALSE;
	}

public:
	
	FCylindricalForceApplicator(const FVector& InOrigin, const FVector& InUpVector, FLOAT InRadius, FLOAT InRadialStrength,
		FLOAT InRotationalStrength, FLOAT InLiftStrength, FLOAT InEscapeVelocity, FLOAT InRadiusTop, 
		FLOAT InLiftFalloffHeight, UBOOL SpecialRadialForce, const FBox& ForceBBox) : 

		Origin(InOrigin), UpVector(InUpVector), Radius(InRadius), RadialStrength(InRadialStrength),
		RotationalStrength(InRotationalStrength), LiftStrength(InLiftStrength), EscapeVelocity(InEscapeVelocity),
		RadiusTop(InRadiusTop), LiftFalloffHeight(InLiftFalloffHeight), bSpecialRadialForce(SpecialRadialForce)
	{
		
	}

	virtual UBOOL ComputeForce(
		FVector* Positions, INT PositionStride, FLOAT PositionScale,
		FVector* Velocities, INT VelocityStride, FLOAT VelocityScale,
		FVector* OutForce, INT OutForceStride, FLOAT OutForceScale,
		FVector* OutTorque, INT OutTorqueStride, FLOAT OutTorqueScale,
		INT Count, const FBox& PositionBoundingBox)
	{
		UBOOL NonZero = FALSE;

		for(INT i = 0; i < Count; i++)
		{
			FVector Position = *Positions * PositionScale;

			if(!IsPointExcluded(Position, PositionBoundingBox))
			{
				FVector Result;
				if(ComputeCylindricalForce(Position, *Velocities * VelocityScale, Result))
				{
					NonZero = TRUE;
					*OutForce += Result * OutForceScale;
				}
			}

			Positions = (FVector *) (((BYTE *)Positions) + PositionStride);
			Velocities = (FVector *) (((BYTE *)Velocities) + VelocityStride);
			OutForce = (FVector *) (((BYTE *)OutForce) + OutForceStride);
		}

		return NonZero;
	}
};


/** In the Tick function, apply forces to anything near this Actor. */
void ARB_CylindricalForceActor::TickSpecial(FLOAT DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_CylindricalForceFieldTick);

	Super::TickSpecial(DeltaSeconds);

	if(bForceActive)
	{
		/* Setup force applcator */
		FVector UpVector = ForceHeight * LocalToWorld().GetAxis(2);
		FVector ApplicatorLocation = Location + (LocalToWorld().GetAxis(2) * HeightOffset) - (0.5f * UpVector);

		/* Compute bounds for overlap query */
		FLOAT MaxRadius = Max(ForceRadius, ForceTopRadius);

		FVector Extent = FVector(MaxRadius, MaxRadius, 0.5f * ForceHeight);
		FVector OffsetVec = FVector(0.0f, 0.0f, HeightOffset);

		FBoxSphereBounds BoxSphereBounds = FBoxSphereBounds( FBox( (-Extent) + OffsetVec, Extent + OffsetVec ) );
		BoxSphereBounds = BoxSphereBounds.TransformBy(LocalToWorld());

		// Create the force applicator
		FCylindricalForceApplicator Applicator( ApplicatorLocation , UpVector, ForceRadius, RadialStrength, RotationalStrength, 
			LiftStrength, EscapeVelocity, ForceTopRadius, LiftFalloffHeight, !bForceApplyToProjectiles,
			BoxSphereBounds.GetBox());

		if(bForceApplyToCloth || bForceApplyToRigidBodies)
		{
			// Query octree for overlapping components
			TArray<UPrimitiveComponent*> TouchingPrimitives;
			GWorld->Hash->GetIntersectingPrimitives(BoxSphereBounds.GetBox(), TouchingPrimitives);

			// Do we need a tighter query than a sphere(could get pretty big if the tornado is tall and thin)?
			// Could use box query for octree, however it doesnt seem used anywhere and is not documented....

			for(INT i=0; i<TouchingPrimitives.Num(); i++)
			{
				UPrimitiveComponent* Comp = TouchingPrimitives(i);
				if(Comp)
				{
					UBOOL bClothForce = bForceApplyToCloth && CollideWithChannels.Cloth;
					UBOOL bRigidBodyForce = bForceApplyToRigidBodies && CheckCollisionChannels(Comp->RBChannel, CollideWithChannels);

					if(bClothForce || bRigidBodyForce)
					{
						Comp->AddForceField( &Applicator, BoxSphereBounds.GetBox(), bClothForce, bRigidBodyForce);
					}
				}
			}
		}

		if(bForceApplyToProjectiles)
		{
			//ApplyProjectileForceField(Applicator);
		}
	}
}
